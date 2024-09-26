#ifndef XV6_FS_NATIVE_FS_H
#define XV6_FS_NATIVE_FS_H

#include "device/device.h"
#include "fsdefs.h"
#include "vfs_fs.h"

void native_iinit();
void native_fs_init(struct vfs_superblock*, struct device*);

struct native_superblock_private {
  struct native_superblock sb;  // in memory copy of superblock for the fs.
  struct device* dev;           // device for the fs.
};

#endif  // XV6_FS_NATIVE_FS_H
