#include "defs.h"
#include "device/device.h"
#include "device/ide_device.h"
#include "device/obj_device.h"
#include "fs/native_fs.h"
#include "fs/obj_fs.h"
#include "mmu.h"
#include "mount.h"
#include "mount_ns.h"
#include "namespace.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

struct {
  struct spinlock mnt_list_lock;  // protects mnt_list
  struct mount_list mnt_list[NMOUNT];
} mount_holder;

struct mount_list *getactivemounts(struct mount_ns *ns) {
  if (ns == NULL) {
    ns = myproc()->nsproxy->mount_ns;
  }
  return ns->active_mounts;
}

static struct mount_list *allocmntlist(void) {
  acquire(&mount_holder.mnt_list_lock);
  int i;
  // Find empty mount struct
  for (i = 0; i < NMOUNT && mount_holder.mnt_list[i].mnt.ref != 0; i++) {
  }

  if (i == NMOUNT) {
    // error - no available mount memory.
    panic("out of mount_list objects");
  }

  struct mount_list *newmountentry = &mount_holder.mnt_list[i];
  newmountentry->mnt.ref = 1;

  release(&mount_holder.mnt_list_lock);

  return newmountentry;
}

// Parent mount (if it exists) must already be ref-incremented.
static int addmountinternal(struct mount_list *mnt_list, struct device *dev,
                            struct vfs_inode *mountpoint, struct mount *parent,
                            struct vfs_inode *bind, struct mount_ns *ns) {
  if (parent != NULL) {
    parent->ref++;
  }
  mnt_list->mnt.parent = parent;
  if (mountpoint) {
    mnt_list->mnt.mountpoint = mountpoint->i_op->idup(mountpoint);
  } else {
    mnt_list->mnt.mountpoint = NULL;
  }

  if (bind != NULL) {
    XV6_ASSERT(dev == NULL);
    mnt_list->mnt.bind = bind->i_op->idup(bind);
    mnt_list->mnt.isbind = XV_TRUE;
  } else {
    XV6_ASSERT(dev != NULL);
    // allocate superblock
    struct vfs_superblock *vfs_sb = sballoc();
    mnt_list->mnt.sb = vfs_sb;
    mnt_list->mnt.isbind = XV_FALSE;
    // initialize filesystem
    switch (dev->type) {
      case DEVICE_TYPE_IDE:
      case DEVICE_TYPE_LOOP:
        native_fs_init(mnt_list->mnt.sb, dev);
        break;
      case DEVICE_TYPE_OBJ:
        obj_fs_init_dev(mnt_list->mnt.sb, dev);
        break;
      default:
        return -1;
    }
    XV6_ASSERT(vfs_sb->ops != NULL);
  }

  // add to linked list
  mnt_list->next = getactivemounts(ns);
  ns->active_mounts = mnt_list;
  return 0;
}

struct mount *getrootmount(void) { return myproc()->nsproxy->mount_ns->root; }

void mntinit(void) {
  initlock(&mount_holder.mnt_list_lock, "mount_list");

  struct mount_list *root_mount = allocmntlist();
  if (root_mount == NULL) {
    panic("failed to allocate root mount");
  }

  struct device *root_dev = get_ide_device(ROOTDEV);
  if (root_dev == NULL) {
    panic("failed to get root device");
  }

  if (addmountinternal(root_mount, root_dev, NULL, NULL, NULL,
                       get_root_mount_ns())) {
    panic("failed to initialize root mount");
  }  // fs start later in init

  deviceput(root_dev);

  // set root mount in namespace
  set_mount_ns_root(get_root_mount_ns(), &root_mount->mnt);
}

struct mount *mntdup(struct mount *mnt) {
  acquire(&mount_holder.mnt_list_lock);
  mnt->ref++;
  release(&mount_holder.mnt_list_lock);
  return mnt;
}

void mntput(struct mount *mnt) {
  XV6_ASSERT(mnt != NULL && mnt->ref > 0);
  acquire(&mount_holder.mnt_list_lock);
  mnt->ref--;
  release(&mount_holder.mnt_list_lock);
}

// mountpoint and bind_dir must be locked.
int mount(struct vfs_inode *mountpoint, struct device *target_dev,
          struct vfs_inode *bind_dir, struct mount *parent) {
  int ret = -1;
  struct mount_list *newmountentry = allocmntlist();
  struct mount *newmount = &newmountentry->mnt;

  // if both target_dev and bind_dir are set, it's an error.
  // but we must have at least one of them.
  if ((target_dev == NULL) == (bind_dir == NULL)) {
    newmount->ref = 0;
    cprintf("mount: must have exactly one of target_dev or bind_dir\n");
    return -1;
  }

  acquire(&myproc()->nsproxy->mount_ns->lock);
  struct mount_list *current = getactivemounts(NULL);
  while (current != 0) {
    if (current->mnt.parent == parent &&
        current->mnt.mountpoint == mountpoint) {
      // error - mount already exists.
      release(&myproc()->nsproxy->mount_ns->lock);
      newmount->ref = 0;
      cprintf("mount already exists at that point.\n");
      goto end;
    }
    current = current->next;
  }

  if (addmountinternal(newmountentry, target_dev, mountpoint, parent, bind_dir,
                       myproc()->nsproxy->mount_ns)) {
    release(&myproc()->nsproxy->mount_ns->lock);

    newmount->ref = 0;
    goto end;
  }

  release(&myproc()->nsproxy->mount_ns->lock);

  if (!newmount->isbind && newmount->sb->ops->start != NULL) {
    newmount->sb->ops->start(newmount->sb);
    XV6_ASSERT(newmount->sb->root_ip != NULL);
  }

  ret = 0;

end:
  return ret;
}

int umount(struct mount *mnt) {
  acquire(&myproc()->nsproxy->mount_ns->lock);
  struct mount_list *current = getactivemounts(NULL);
  struct mount_list **previous = &(myproc()->nsproxy->mount_ns->active_mounts);
  while (current != 0) {
    if (&current->mnt == mnt) {
      break;
    }
    previous = &current->next;
    current = current->next;
  }

  if (current == 0) {
    // error - not actually mounted.
    release(&myproc()->nsproxy->mount_ns->lock);
    cprintf("current=0\n");
    return -1;
  }

  const XV_Bool is_root_mount = current->mnt.parent == NULL;
  // sanity -- root mount has no attached mountpoint.
  XV6_ASSERT(!is_root_mount || current->mnt.mountpoint == NULL);

  acquire(&mount_holder.mnt_list_lock);

  // Base ref is 1, +1 for the mount being acquired before entering this method.
  if (current->mnt.ref > 2) {
    cprintf("current->mnt(%d).refs=%d>1\n", &current->mnt,
            current->mnt.ref - 1);
    // error - can't unmount as there are references.
    release(&mount_holder.mnt_list_lock);
    release(&myproc()->nsproxy->mount_ns->lock);
    return -1;
  }

  // remove from linked list
  *previous = current->next;
  release(&myproc()->nsproxy->mount_ns->lock);

  struct vfs_inode *oldmountpoint = current->mnt.mountpoint;
  struct vfs_inode *oldbind = current->mnt.isbind ? current->mnt.bind : NULL;
  struct vfs_superblock *sb = !current->mnt.isbind ? current->mnt.sb : NULL;

  current->mnt.bind = NULL;
  current->mnt.mountpoint = NULL;
  if (!is_root_mount) {
    current->mnt.parent->ref--;
  }
  current->mnt.ref = 0;
  current->next = NULL;

  release(&mount_holder.mnt_list_lock);

  if (oldbind) {
    oldbind->i_op->iput(oldbind);
  }

  if (!is_root_mount) {
    XV6_ASSERT(oldmountpoint != NULL);
    oldmountpoint->i_op->iput(oldmountpoint);
  }

  if (sb) {
    sbput(sb);
  }
  return 0;
}

struct mount *mntlookup(struct vfs_inode *mountpoint, struct mount *parent) {
  acquire(&myproc()->nsproxy->mount_ns->lock);

  struct mount_list *entry = getactivemounts(NULL);
  while (entry != 0) {
    /* Search for a matching mountpoint and also a parent mount, unless it is a
     * bind mount which inherently has different parents. */
    if (entry->mnt.mountpoint == mountpoint &&
        (entry->mnt.parent == parent || entry->mnt.isbind)) {
      release(&myproc()->nsproxy->mount_ns->lock);
      return mntdup(&entry->mnt);
    }
    entry = entry->next;
  }

  release(&myproc()->nsproxy->mount_ns->lock);
  return 0;
}

void umountall(struct mount_list *mounts) {
  int umount_ret = -1;

  while (mounts != NULL) {
    struct mount_list *next = mounts->next;
    if (mounts->mnt.parent != NULL) {
      begin_op();
      umount_ret = umount(&mounts->mnt);
      end_op();
      if (umount_ret) {
        panic("failed to umount upon namespace close");
      }
    } else {
      XV6_ASSERT(myproc()->nsproxy->mount_ns->root == &mounts->mnt);
    }
    mounts = next;
  }

  // umount root.
  begin_op();
  umount_ret = umount(myproc()->nsproxy->mount_ns->root);
  end_op();
  if (umount_ret) {
    panic("failed to umount upon namespace close");
  }
}

static struct mount_list *shallowcopyactivemounts(struct mount **newcwdmount) {
  struct mount_list *head = NULL;
  struct mount_list *entry = myproc()->nsproxy->mount_ns->active_mounts;
  struct mount_list *prev = NULL;
  while (entry != 0) {
    struct mount_list *newentry = allocmntlist();
    if (head == 0) {
      head = newentry;
    }
    newentry->mnt.ref = 1;
    if (entry->mnt.mountpoint != 0) {
      newentry->mnt.mountpoint =
          entry->mnt.mountpoint->i_op->idup(entry->mnt.mountpoint);
    } else {
      newentry->mnt.mountpoint = 0;
    }
    newentry->mnt.parent = 0;
    newentry->mnt.isbind = entry->mnt.isbind;
    if (entry->mnt.isbind) {
      XV6_ASSERT(entry->mnt.bind != 0);
      newentry->mnt.bind = entry->mnt.bind->i_op->idup(entry->mnt.bind);
    } else {
      XV6_ASSERT(entry->mnt.sb != 0);
      sbdup(entry->mnt.sb);
      newentry->mnt.sb = entry->mnt.sb;
    }
    if (prev != 0) {
      prev->next = newentry;
    }

    if (myproc()->cwdmount == &entry->mnt) {
      *newcwdmount = &newentry->mnt;
    }

    prev = newentry;
    entry = entry->next;
  }
  if (prev != NULL) {
    prev->next = NULL;
  }
  return head;
}

static inline void print_mount_full_info(const struct mount *const m) {
  cprintf(
      "mount: %p, parent: %p, mountpoint: %p, ref: %d, isbind: %d sb: %p bind "
      "%p\n",
      m, m->parent, m->mountpoint, m->ref, m->isbind, m->sb, m->bind);
}

static void fixparents(struct mount_list *const new_active_mounts) {
  struct mount_list *new_current = new_active_mounts;
  const struct mount_list *old_current =
      myproc()->nsproxy->mount_ns->active_mounts;

  while (old_current != 0) {
    if (old_current->mnt.parent != 0) {
      struct mount_list *old_found_parent =
          myproc()->nsproxy->mount_ns->active_mounts;
      struct mount_list *new_found_parent = new_active_mounts;
      while (old_found_parent != 0 &&
             old_current->mnt.parent != &old_found_parent->mnt) {
        old_found_parent = old_found_parent->next;
        new_found_parent = new_found_parent->next;
      }

      XV6_ASSERT(old_found_parent != 0 && new_found_parent != 0);
      XV6_ASSERT(old_found_parent->mnt.isbind == new_found_parent->mnt.isbind);
      // same assertion as below for parents.
      if (old_found_parent->mnt.isbind) {
        XV6_ASSERT(old_found_parent->mnt.bind == old_found_parent->mnt.bind)
      } else {
        XV6_ASSERT(old_found_parent->mnt.sb == old_found_parent->mnt.sb)
      }
      XV6_ASSERT(new_current->mnt.parent == NULL);
      new_current->mnt.parent = mntdup(&new_found_parent->mnt);
    }

    XV6_ASSERT(old_current->mnt.isbind == new_current->mnt.isbind);
    if (old_current->mnt.isbind) {
      XV6_ASSERT(old_current->mnt.bind == new_current->mnt.bind &&
                 old_current->mnt.bind != 0);
    } else {
      XV6_ASSERT(old_current->mnt.sb == new_current->mnt.sb &&
                 old_current->mnt.sb != 0);
    }

    new_current = new_current->next;
    old_current = old_current->next;
  }
}

struct mount_list *copyactivemounts(void) {
  acquire(&myproc()->nsproxy->mount_ns->lock);
  struct mount *oldcwdmount = myproc()->cwdmount;
  struct mount *newcwdmount = 0;
  struct mount_list *newentry = shallowcopyactivemounts(&newcwdmount);
  fixparents(newentry);
  release(&myproc()->nsproxy->mount_ns->lock);
  if (newcwdmount != 0) {
    myproc()->cwdmount = mntdup(newcwdmount);
    mntput(oldcwdmount);
  }
  return newentry;
}

struct mount *getroot(struct mount_list *newentry) {
  if (newentry != 0) {
    struct mount *current = &newentry->mnt;
    while (current != 0 && current->parent != 0) {
      current = current->parent;
    }

    if (current == 0) {
      panic("malformed mount structure - no root");
    }

    return current;
  }

  return 0;
}

// get the root inode of the provided mount, and increment its ref count
struct vfs_inode *get_mount_root_ip(struct mount *m) {
  struct vfs_inode *next;
  if (m->isbind) {
    XV6_ASSERT(m->bind != 0);
    next = m->bind;
  } else {
    XV6_ASSERT(m->sb != 0);
    next = m->sb->root_ip;
  }
  return next->i_op->idup(next);
}

int pivot_root(struct vfs_inode *new_root, struct mount *new_root_mount,
               struct vfs_inode *put_old_root_inode,
               struct mount *put_old_root_inode_mnt) {
  int status = -1;
  // Check that new_root_mount is not the root of the current mount.
  if (new_root_mount == getrootmount()) {
    cprintf("new_root_mount == getrootmount()\n");
    return -1;
  }

  // Check that new_root is the root of the new mount.
  struct vfs_inode *entry = get_mount_root_ip(new_root_mount);
  XV_Bool is_matching = entry == new_root;
  entry->i_op->iput(entry);
  if (!is_matching) {
    cprintf("entry != new_root_inode\n");
    return -1;
  }

  if (!vfs_is_child_of(new_root, new_root_mount, put_old_root_inode,
                       put_old_root_inode_mnt)) {
    cprintf("new_root is not a child of put_old_root_inode\n");
    return -1;
  }

  acquire(&mount_holder.mnt_list_lock);

  struct mount *old_root = getrootmount();
  new_root_mount->ref++;
  old_root->parent = put_old_root_inode_mnt;
  set_mount_ns_root(myproc()->nsproxy->mount_ns, new_root_mount);

  if (new_root_mount->parent != NULL) {
    new_root_mount->parent->ref--;
    new_root_mount->parent = NULL;
  }

  if (new_root_mount->mountpoint != NULL) {
    new_root_mount->mountpoint->i_op->iput(new_root_mount->mountpoint);
    new_root_mount->mountpoint = NULL;
  }

  // Finally, attach old root to it's new mountpoint.
  old_root->mountpoint = put_old_root_inode->i_op->idup(put_old_root_inode);

  status = 0;

  release(&mount_holder.mnt_list_lock);
  return status;
}
