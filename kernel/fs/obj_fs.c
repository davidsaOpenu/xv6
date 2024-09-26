// Object File system implementation.
// This file contains the low-level file system manipulation
// routines.  The (higher-level) system call implementations
// are in sysfile.c.

#include "obj_fs.h"

#include "defs.h"
#include "device/buf.h"
#include "device/device.h"
#include "device/obj_cache.h"
#include "device/obj_disk.h"  // for error codes and `new_inode_number`
#include "kvector.h"
#include "mmu.h"
#include "mount.h"
#include "obj_file.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"
#include "vfs_fs.h"

static const struct inode_operations obj_inode_ops;
static const struct sb_ops obj_ops;

struct {
  struct spinlock lock;
  struct obj_inode inode[NINODE];
} obj_icache;

void obj_iinit() {
  initlock(&obj_icache.lock, "obj_icache");
  for (uint i = 0; i < NINODE; i++) {
    initsleeplock(&obj_icache.inode[i].vfs_inode.lock, "obj_inode");
    obj_icache.inode[i].vfs_inode.ref = 0;
    obj_icache.inode[i].vfs_inode.valid = 0;
  }
}

void inode_name(char *output, uint inum) {
  const char *prefix = "inode";
  memmove(output, prefix, strlen(prefix));
  for (uint i = 0; i <= sizeof(uint); i++) {
    output[i + strlen(prefix)] = inum % 127 + 128;
    inum /= 127;
  }
  output[strlen(prefix) + sizeof(uint) + 1] = 0;  // null terminator
}

void file_name(char *output, uint inum) {
  for (uint i = 0; i <= sizeof(uint); i++) {
    output[i] = inum % 127 + 128;
    inum /= 127;
  }
  output[sizeof(uint) + 1] = 0;  // null terminator
}

// PAGEBREAK!
//  Allocate an object and its corresponding inode object to the device object
//  table. Returns an unlocked but allocated and referenced inode.
static struct vfs_inode *obj_ialloc(struct vfs_superblock *sb, file_type type) {
  struct device *const dev = sb_private(sb);
  int inum = new_inode_number(dev);
  char iname[INODE_NAME_LENGTH];
  struct obj_dinode di = {0};
  struct vfs_inode *ip;

  di.base_dinode.type = type;
  di.base_dinode.nlink = 0;
  if (type == T_DEV) {
    di.data_object_name[0] = 0;
  } else {
    file_name(di.data_object_name, inum);
  }
  inode_name(iname, inum);
  if (obj_cache_add(dev, iname, &di, sizeof(di)) != NO_ERR) {
    panic("obj_ialloc: failed adding inode to disk");
  }

  if (type != T_DEV) {
    if (obj_cache_add(dev, di.data_object_name, 0, 0) != NO_ERR) {
      panic("obj_ialloc: failed adding object to disk");
    }
  }

  ip = obj_ops.iget(sb, inum);

  return ip;
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void obj_iupdate(struct vfs_inode *vfs_ip) {
  struct obj_dinode di;
  struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);
  struct device *const dev = vfs_ip->sb->private;
  char iname[INODE_NAME_LENGTH];
  inode_name(iname, ip->vfs_inode.inum);
  di.base_dinode.type = ip->vfs_inode.type;
  di.base_dinode.major = ip->vfs_inode.major;
  di.base_dinode.minor = ip->vfs_inode.minor;
  di.base_dinode.nlink = ip->vfs_inode.nlink;
  memmove(di.data_object_name, ip->data_object_name, MAX_OBJECT_NAME_LENGTH);
  if (obj_cache_write(dev, iname, (void *)&di, sizeof(di), 0, sizeof(di)) !=
      NO_ERR) {
    panic("obj_iupdate: failed writing dinode to the disk");
  }
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct vfs_inode *obj_iget(struct vfs_superblock *sb, uint inum) {
  struct obj_inode *ip, *empty;

  acquire(&obj_icache.lock);

  // Is the inode already cached?
  empty = 0;
  for (ip = &obj_icache.inode[0]; ip < &obj_icache.inode[NINODE]; ip++) {
    if (ip->vfs_inode.ref > 0 && ip->vfs_inode.sb == sb &&
        ip->vfs_inode.inum == inum) {
      ip->vfs_inode.ref++;
      release(&obj_icache.lock);
      return &ip->vfs_inode;
    }
    if (empty == 0 && ip->vfs_inode.ref == 0)  // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if (empty == 0) panic("iget: no inodes");

  ip = empty;
  ip->vfs_inode.sb = sb;
  ip->vfs_inode.inum = inum;
  ip->vfs_inode.ref = 1;
  ip->vfs_inode.valid = 0;

  ip->data_object_name[0] = 0;
  file_name(ip->data_object_name, inum);

  struct device *const dev = sb_private(sb);
  deviceget(dev);

  /* Initiate inode operations for obj fs */
  ip->vfs_inode.i_op = &obj_inode_ops;

  release(&obj_icache.lock);

  return &ip->vfs_inode;
}

void obj_iput(struct vfs_inode *vfs_ip);

static void obj_fsdestroy(struct vfs_superblock *vfs_sb) {
  struct vfs_inode *root_ip = vfs_sb->root_ip;
  obj_iput(root_ip);
  deviceput(vfs_sb->private);
  vfs_sb->root_ip = NULL;
  vfs_sb->ops = NULL;
  vfs_sb->private = NULL;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct vfs_inode *obj_idup(struct vfs_inode *ip) {
  acquire(&obj_icache.lock);
  ip->ref++;
  release(&obj_icache.lock);
  return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void obj_ilock(struct vfs_inode *vfs_ip) {
  struct obj_dinode di = {0};
  struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);
  char iname[INODE_NAME_LENGTH];

  if (ip == 0 || ip->vfs_inode.ref < 1) panic("obj_ilock");
  struct device *const dev = sb_private(ip->vfs_inode.sb);
  acquiresleep(&ip->vfs_inode.lock);
  if (ip->vfs_inode.valid == 0) {
    inode_name(iname, ip->vfs_inode.inum);
    vector div = newvector(sizeof(di), 1);
    if (obj_cache_read(dev, iname, &div, sizeof(di), 0, sizeof(di)) != NO_ERR) {
      panic("inode doesn't exists in the disk");
    }
    memmove_from_vector((char *)&di, div, 0, div.vectorsize);
    ip->vfs_inode.type = di.base_dinode.type;
    ip->vfs_inode.major = di.base_dinode.major;
    ip->vfs_inode.minor = di.base_dinode.minor;
    ip->vfs_inode.nlink = di.base_dinode.nlink;
    if (di.base_dinode.type == T_DEV) {
      ip->vfs_inode.size = 0;
    } else {
      if (object_size(dev, di.data_object_name, &ip->vfs_inode.size) !=
          NO_ERR) {
        panic("object doesn't exists in the disk");
      }
    }
    memmove(ip->data_object_name, di.data_object_name, MAX_OBJECT_NAME_LENGTH);
    ip->vfs_inode.valid = 1;
    if (ip->vfs_inode.type == 0) panic("obj_ilock: no type");
  }
}

// Deletes inode and it's content from the disk.
static void idelete(struct obj_inode *ip) {
  if (ip->data_object_name[0] != 0) {
    struct device *const dev = sb_private(ip->vfs_inode.sb);
    if (obj_cache_delete(dev, ip->data_object_name, ip->vfs_inode.size) !=
        NO_ERR) {
      panic("idelete: failed to delete content object");
    }
    ip->data_object_name[0] = 0;
  }

  char iname[INODE_NAME_LENGTH];
  inode_name(iname, ip->vfs_inode.inum);
  struct device *const dev = sb_private(ip->vfs_inode.sb);
  if (obj_cache_delete(dev, iname, sizeof(struct obj_dinode)) != NO_ERR) {
    panic("idelete: failed to delete inode object");
  }
}

// Unlock the given inode.
void obj_iunlock(struct vfs_inode *ip) {
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1) panic("obj_iunlock");

  releasesleep(&ip->lock);
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void obj_iput(struct vfs_inode *vfs_ip) {
  struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

  acquiresleep(&ip->vfs_inode.lock);
  if (ip->vfs_inode.valid && ip->vfs_inode.nlink == 0) {
    acquire(&obj_icache.lock);
    int r = ip->vfs_inode.ref;
    release(&obj_icache.lock);
    if (r == 1) {
      // inode has no links and no other references: truncate and free.
      idelete(ip);
      ip->vfs_inode.type = 0;
      ip->vfs_inode.valid = 0;
    }
  }
  releasesleep(&ip->vfs_inode.lock);
  acquire(&obj_icache.lock);

  ip->vfs_inode.ref--;
  if (ip->vfs_inode.ref == 0) {
    struct device *const dev = sb_private(ip->vfs_inode.sb);
    deviceput(dev);
  }
  release(&obj_icache.lock);
}

// Common idiom: unlock, then put.
void obj_iunlockput(struct vfs_inode *ip) {
  obj_iunlock(ip);
  obj_iput(ip);
}

// PAGEBREAK!
//  Inode content

// Copy stat information from inode.
// Caller must hold ip->lock.
void obj_stati(struct vfs_inode *vfs_ip, struct stat *st) {
  struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);
  const struct device *const dev = sb_private(ip->vfs_inode.sb);
  st->dev = dev->id;
  st->ino = ip->vfs_inode.inum;
  st->type = ip->vfs_inode.type;
  st->nlink = ip->vfs_inode.nlink;
  st->size = ip->vfs_inode.size;
}

// PAGEBREAK!
//  Read data from inode.
//  Caller must hold ip->lock.
int obj_readi(struct vfs_inode *vfs_ip, uint off, uint n, vector *dstvector) {
  struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

  if (ip->vfs_inode.type == T_DEV) {
    if (ip->vfs_inode.major < 0 || ip->vfs_inode.major >= NDEV ||
        !devsw[ip->vfs_inode.major].read)
      return -1;
    unsigned int read_result =
        devsw[ip->vfs_inode.major].read(vfs_ip, n, dstvector);
    return read_result;
  }

  if (ip->data_object_name[0] == 0) {
    panic("obj_readi reading from inode without data object");
  }
  if (off > vfs_ip->size || off + n < off) return -1;
  if (off + n > vfs_ip->size) n = vfs_ip->size - off;
  if (0 == n) return 0;

  struct device *const dev = sb_private(ip->vfs_inode.sb);
  if (obj_cache_read(dev, ip->data_object_name, dstvector, n, off,
                     vfs_ip->size) != NO_ERR) {
    panic("obj_readi failed reading object content");
  }
  return n;
}

// PAGEBREAK!
// Write data to inode.
// Caller must hold ip->lock.
int obj_writei(struct vfs_inode *vfs_ip, char *src, uint off, uint n) {
  struct obj_inode *ip = container_of(vfs_ip, struct obj_inode, vfs_inode);

  if (ip->vfs_inode.type == T_DEV) {
    if (ip->vfs_inode.major < 0 || ip->vfs_inode.major >= NDEV ||
        !devsw[ip->vfs_inode.major].write)
      return -1;
    return devsw[ip->vfs_inode.major].write(vfs_ip, src, n);
  }

  if (ip->data_object_name[0] == 0) {
    panic("obj writei writing to inode without data object");
  }

  if (off > vfs_ip->size || off + n < off) return -1;
  if (off + n > MAX_INODE_OBJECT_DATA) return -1;

  struct device *const dev = sb_private(ip->vfs_inode.sb);
  if (obj_cache_write(dev, ip->data_object_name, src, n, off, vfs_ip->size) !=
      NO_ERR) {
    panic("obj_writei failed to write object content");
  }

  if (vfs_ip->size < off + n) {
    vfs_ip->size = off + n;
  }

  return n;
}

// PAGEBREAK!
//  Directories

// Is the directory dp empty except for "." and ".." ?
int obj_isdirempty(struct vfs_inode *vfs_dp) {
  int off;
  struct dirent de;
  vector direntryvec;
  struct obj_inode *dp = container_of(vfs_dp, struct obj_inode, vfs_inode);
  direntryvec = newvector(sizeof(de), 1);

  for (off = 2 * sizeof(de); off < vfs_dp->size; off += sizeof(de)) {
    if (dp->vfs_inode.i_op->readi(&dp->vfs_inode, off, sizeof(de),
                                  &direntryvec) != sizeof(de))
      panic("obj_isdirempty: readi");
    // vectormemcmp("obj_isdirempty",direntryvec,off,(char*)&de,sizeof(de));
    memmove_from_vector((char *)&de, direntryvec, 0, sizeof(de));
    if (de.inum != 0) {
      freevector(&direntryvec);
      return 0;
    }
  }
  freevector(&direntryvec);
  return 1;
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct vfs_inode *obj_dirlookup(struct vfs_inode *vfs_dp, char *name,
                                uint *poff) {
  uint off;
  struct dirent de;
  struct obj_inode *dp = container_of(vfs_dp, struct obj_inode, vfs_inode);
  vector direntryvec;
  direntryvec = newvector(sizeof(de), 1);

  if (dp->vfs_inode.type != T_DIR) panic("obj_dirlookup not DIR");

  if (dp->data_object_name[0] == 0) {
    panic("ob_dirlookup received inode without data");
  }

  for (off = 0; off < vfs_dp->size; off += sizeof(de)) {
    if (obj_readi(&dp->vfs_inode, off, sizeof(de), &direntryvec) != sizeof(de))
      panic("obj_dirlookup read");
    memmove_from_vector((char *)&de, direntryvec, 0, sizeof(de));
    if (de.inum == 0) continue;
    if (vfs_namecmp(name, de.name) == 0) {
      // entry matches path element
      if (poff) *poff = off;
      freevector(&direntryvec);
      return dp->vfs_inode.sb->ops->iget(dp->vfs_inode.sb, de.inum);
    }
  }
  freevector(&direntryvec);
  return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int obj_dirlink(struct vfs_inode *vfs_dp, char *name, uint inum) {
  int off;
  struct dirent de;
  struct vfs_inode *ip;
  struct obj_inode *dp = container_of(vfs_dp, struct obj_inode, vfs_inode);
  char iname[INODE_NAME_LENGTH];
  struct obj_dinode di;
  vector direntryvec;
  direntryvec = newvector(sizeof(de), 1);

  // Check that name is not present.
  if ((ip = obj_dirlookup(&dp->vfs_inode, name, 0)) != 0) {
    obj_iput(ip);
    return -1;
  }
  if (dp->data_object_name[0] == 0) {
    panic("obj_dirlink received inode without data");
  }

  memset(&de, 0, sizeof(de));

  // Look for an empty dirent.
  for (off = 0; off < vfs_dp->size; off += sizeof(de)) {
    if (obj_readi(&dp->vfs_inode, off, sizeof(de), &direntryvec) != sizeof(de))
      panic("obj_dirlink read");
    memmove_from_vector((char *)&de, direntryvec, 0, sizeof(de));
    if (de.inum == 0) break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  inode_name(iname, inum);
  memset(&di, 0, sizeof(di));
  vector div = newvector(sizeof(di), 1);

  struct device *const dev = sb_private(vfs_dp->sb);
  if (obj_cache_read(dev, iname, &div, sizeof(di), 0, sizeof(di)) != NO_ERR) {
    panic("inode doesn't exists in the disk");
  }
  memmove_from_vector((char *)&di, div, 0, div.vectorsize);
  if (obj_writei(&dp->vfs_inode, (char *)&de, off, sizeof(de)) != sizeof(de))
    panic("obj_dirlink");

  freevector(&direntryvec);
  return 0;
}

void obj_fs_init(struct vfs_superblock *vfs_sb, struct device *dev) {
  struct vfs_inode *root_inode;
  struct dirent de;
  uint off = 0;

  deviceget(dev);
  vfs_sb->private = dev;
  vfs_sb->ops = &obj_ops;

  acquire(&obj_icache.lock);
  for (uint i = 0; i < NINODE; i++) {
    if (obj_icache.inode[i].vfs_inode.sb == vfs_sb) {
      initsleeplock(&obj_icache.inode[i].vfs_inode.lock, "obj_inode");
      obj_icache.inode[i].vfs_inode.ref = 0;
      obj_icache.inode[i].vfs_inode.valid = 0;
    }
  }
  release(&obj_icache.lock);

  /* Initiate root dir */
  root_inode = obj_ialloc(vfs_sb, T_DIR);
  obj_ilock(root_inode);

  if (root_inode->inum != OBJ_ROOTINO) {
    panic("Obj device is not in initial state");
  }

  // Prevent the root dir from being freed.
  root_inode->nlink++;
  root_inode->i_op->iupdate(root_inode);

  /* Initiate inode for root dir */
  strncpy(de.name, ".", DIRSIZ);
  de.inum = root_inode->inum;

  if (obj_writei(root_inode, (char *)&de, off, sizeof(de)) != sizeof(de)) {
    panic("Couldn't create root dir in obj fs");
  }

  off += sizeof(de);
  strncpy(de.name, "..", DIRSIZ);
  de.inum = root_inode->inum;

  if (obj_writei(root_inode, (char *)&de, off, sizeof(de)) != sizeof(de)) {
    panic("Couldn't create root dir in obj fs");
  }

  vfs_sb->root_ip = root_inode;

  obj_iunlock(root_inode);

  /* For tries, TODO: need to move this logic to obj mkfs (and create one) */
  //    ip = obj_ialloc(dev, T_FILE);
  //    off += sizeof(de);
  //    strncpy(de.name, "o1", DIRSIZ);
  //    de.inum = ip->inum;
  //    if (obj_writei(root_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
  //    {
  //        panic("Couldn't create root dir in obj fs");
  //    }
  //
  //    ip = obj_ialloc(dev, T_DIR);
  //    off += sizeof(de);
  //    strncpy(de.name, "o2", DIRSIZ);
  //    de.inum = ip->inum;
  //    if (obj_writei(root_inode, (char *) &de, off, sizeof(de)) != sizeof(de))
  //    {
  //        panic("Couldn't create root dir in obj fs");
  //    }
}

static const struct sb_ops obj_ops = {.ialloc = obj_ialloc,
                                      .iget = obj_iget,
                                      .destroy = obj_fsdestroy,
                                      .start = NULL};

static const struct inode_operations obj_inode_ops = {
    .idup = &obj_idup,
    .iupdate = &obj_iupdate,
    .iput = &obj_iput,
    .dirlink = &obj_dirlink,
    .dirlookup = &obj_dirlookup,
    .ilock = &obj_ilock,
    .iunlock = &obj_iunlock,
    .readi = &obj_readi,
    .stati = &obj_stati,
    .writei = &obj_writei,
    .iunlockput = &obj_iunlockput,
    .isdirempty = &obj_isdirempty,
};
