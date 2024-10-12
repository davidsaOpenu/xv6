// #include "defs.h"
#include "vfs_fs.h"

#include "device/device.h"
#include "mount.h"
#include "obj_fs.h"
#include "proc.h"

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *vfs_skipelem(char *path, char *name) {
  char *s;
  int len;

  while (*path == '/') path++;
  if (*path == 0) return 0;
  s = path;
  while (*path != '/' && *path != 0) path++;
  len = path - s;
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/') path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct vfs_inode *vfs_namex(char *path, int nameiparent, char *name,
                                   struct mount **mnt,
                                   struct vfs_inode *startip,
                                   struct mount *startmnt) {
  struct vfs_inode *ip, *next;
  struct mount *curmount = NULL;

  XV6_ASSERT((startip == NULL) ==
             (startmnt == NULL));  // both null or both non-null
  if (startip != NULL) {
    curmount = mntdup(startmnt);
    ip = startip->i_op->idup(startip);
  } else {
    if (*path == '/') {
      curmount = mntdup(getrootmount());
      ip = get_mount_root_ip(curmount);
    } else {
      curmount = mntdup(myproc()->cwdmount);
      ip = myproc()->cwd->i_op->idup(myproc()->cwd);
    }
  }

  while ((path = vfs_skipelem(path, name)) != 0) {
    ip->i_op->ilock(ip);
    if (ip->type != T_DIR) {
      ip->i_op->iunlockput(ip);
      mntput(curmount);
      return 0;
    }
    if (nameiparent && *path == '\0') {
      // Stop one level early.
      ip->i_op->iunlock(ip);
      *mnt = curmount;
      return ip;
    }

    if ((next = ip->i_op->dirlookup(ip, name, 0)) == 0) {
      ip->i_op->iunlockput(ip);
      mntput(curmount);
      return 0;
    }

    ip->i_op->iunlockput(ip);

    if ((!vfs_namencmp(name, "..", 3)) && curmount != 0 &&
        (curmount != getrootmount()) && (ip == curmount->sb->root_ip) &&
        curmount->mountpoint != 0 &&
        curmount->mountpoint->i_op->dirlookup != NULL) {
      // valid ".." path component lookup
      struct mount *nextmount = mntdup(curmount->parent);
      next->i_op->iput(next);
      next =
          curmount->mountpoint->i_op->dirlookup(curmount->mountpoint, "..", 0);
      mntput(curmount);
      curmount = nextmount;
    } else {
      // check if next is a mountpoint, and if so, switch to that mountpoint
      struct mount *nextmount = mntlookup(next, curmount);
      if (nextmount != NULL) {
        next->i_op->iput(next);
        next = get_mount_root_ip(nextmount);
        mntput(curmount);
        curmount = nextmount;
      }
    }

    ip = next;
  }

  if (nameiparent) {
    ip->i_op->iput(ip);
    mntput(curmount);
    return 0;
  }

  *mnt = curmount;
  return ip;
}  // NOLINT

struct vfs_inode *vfs_namei(char *path) {
  char name[DIRSIZ];
  struct mount *mnt;
  struct vfs_inode *ip = vfs_namex(path, 0, name, &mnt, NULL, NULL);

  if (ip != 0) {
    mntput(mnt);
  }

  return ip;
}

struct vfs_inode *vfs_nameiparent(char *path, char *name) {
  struct mount *mnt;
  struct vfs_inode *ip = vfs_namex(path, 1, name, &mnt, NULL, NULL);
  if (ip != 0) {
    mntput(mnt);
  }

  return ip;
}

struct vfs_inode *vfs_nameiparentmount(char *path, char *name,
                                       struct mount **mnt) {
  return vfs_namex(path, 1, name, mnt, NULL, NULL);
}

struct vfs_inode *vfs_nameimount(char *path, struct mount **mnt) {
  char name[DIRSIZ];
  return vfs_namex(path, 0, name, mnt, NULL, NULL);
}

static struct vfs_inode *vfs_igetparent(struct vfs_inode *ip, struct mount *mnt,
                                        struct mount **mnt_out) {
  struct vfs_inode *parent = vfs_namex("..", 0, NULL, mnt_out, ip, mnt);
  return parent;
}

bool vfs_is_child_of(struct vfs_inode *parent, struct mount *parentmnt,
                     struct vfs_inode *child, struct mount *childmnt) {
  struct vfs_inode *cur = child->i_op->idup(child);
  struct mount *curmnt = mntdup(childmnt);
  while (cur != NULL) {
    if (cur == parent && curmnt == parentmnt) {
      mntput(curmnt);
      cur->i_op->iput(cur);
      return true;
    }
    if (curmnt == getrootmount()) {
      struct vfs_inode *mnt_root_ip = get_mount_root_ip(curmnt);
      mnt_root_ip->i_op->iput(mnt_root_ip);
      if (cur == mnt_root_ip) {
        break;
      }
    }
    struct vfs_inode *next;
    struct mount *nextmnt;
    next = vfs_igetparent(cur, curmnt, &nextmnt);
    cur->i_op->iput(cur);
    mntput(curmnt);
    cur = next;
    curmnt = nextmnt;
  }
  if (cur != NULL) {
    cur->i_op->iput(cur);
  }
  mntput(curmnt);
  return false;
}

int vfs_namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

int vfs_namencmp(const char *s, const char *t, int length) {
  return strncmp(s, t, length);
}

struct vfs_superblock *sballoc() {
  struct vfs_superblock *sb = (struct vfs_superblock *)kalloc();
  if (sb == 0) {
    return 0;
  }
  memset(sb, 0, sizeof(*sb));
  initlock(&sb->lock, "vfs_sb");
  sb->ref = 1;
  return sb;
}

void sbdup(struct vfs_superblock *sb) {
  XV6_ASSERT(sb->ref > 0);
  acquire(&sb->lock);
  sb->ref++;
  release(&sb->lock);
}

void sbput(struct vfs_superblock *sb) {
  XV6_ASSERT(sb->ref > 0);
  acquire(&sb->lock);
  sb->ref--;
  release(&sb->lock);
  if (sb->ref == 0) {
    // teardown the filesystem
    sb->ops->destroy(sb);
    // and release the superblock
    kfree((char *)sb);
  }
}
