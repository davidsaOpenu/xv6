#ifndef XV6_DEVICE_LOOP_DEVICE_H
#define XV6_DEVICE_LOOP_DEVICE_H

#include "device.h"
#include "fs/vfs_file.h"

struct device* create_loop_device(struct vfs_inode*);
struct device* get_loop_device(const struct vfs_inode*);

#endif  // XV6_DEVICE_LOOP_DEVICE_H
