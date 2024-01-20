#ifndef XV6_MOUNT_H
#define XV6_MOUNT_H

#include "types.h"

struct mount {
  struct mount *parent;
  struct vfs_inode *mountpoint;
  int ref;
  uint dev;
  struct vfs_inode *bind;
};

#define NMOUNT (200)

#endif /* XV6_MOUNT_H */
