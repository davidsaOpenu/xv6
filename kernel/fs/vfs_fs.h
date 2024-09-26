#ifndef XV6_VFS_FS_H
#define XV6_VFS_FS_H

#include "fsdefs.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

struct vfs_superblock;

struct sb_ops {
  struct vfs_inode *(*ialloc)(struct vfs_superblock *sb, file_type type);
  struct vfs_inode *(*iget)(struct vfs_superblock *sb, uint inum);
  void (*start)(struct vfs_superblock *sb);
  void (*destroy)(struct vfs_superblock *sb);
};

struct vfs_superblock {
  int ref;
  struct spinlock lock;
  void *private;
  const struct sb_ops *ops;
  struct vfs_inode *root_ip;
};

static inline void *sb_private(struct vfs_superblock *sb) {
  return sb->private;
}

#endif /* XV6_VFS_FS_H */
