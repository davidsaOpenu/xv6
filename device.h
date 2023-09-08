#ifndef XV6_DEVICE_H
#define XV6_DEVICE_H

#include "fs.h"
#include "obj_fs.h"
#include "spinlock.h"

#define LOOP_DEVICE_DEV (7)
#define OBJ_DEV (15)
#define DEV_TO_LOOP_DEVICE(dev) ((dev)&0xffff)
#define DEV_TO_OBJ_DEVICE(dev) ((dev)&0xffff)
#define LOOP_DEVICE_TO_DEV(ld) ((ld) | (LOOP_DEVICE_DEV << 16))
#define OBJ_TO_DEV(ld) ((ld) | (OBJ_DEV << 16))
#define IS_LOOP_DEVICE(dev) (((dev) >> 16) == LOOP_DEVICE_DEV)
#define IS_OBJ_DEVICE(dev) (((dev) >> 16) == OBJ_DEV)

#define NLOOPDEVS (10)
#define NIDEDEVS (2)
#define NOBJDEVS (2)

struct device {
  struct superblock sb;
  int ref;
  struct vfs_inode *ip;
};

struct obj_device {
  struct objsuperblock sb;
  int ref;
  struct vfs_inode *root_ip;
};

struct dev_holder_s {
  struct spinlock lock;  // protects loopdevs
  struct device loopdevs[NLOOPDEVS];
  struct superblock idesb[NIDEDEVS];
  struct obj_device objdev[NOBJDEVS];
};
extern struct dev_holder_s dev_holder;

#endif /* XV6_DEVICE_H */
