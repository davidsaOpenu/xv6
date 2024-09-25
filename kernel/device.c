#include "device.h"

#include "defs.h"
#include "file.h"
#include "obj_disk.h"
#include "sleeplock.h"
#include "types.h"

struct dev_holder_s dev_holder = {0};

void devinit(void) {
  int i = 0;
  initlock(&dev_holder.lock, "dev_list");

  // Initiate ref of obj device for future use. (if the device will be save as
  // internal_fs files and not in the ram)
  for (i = 0; i < NOBJDEVS; i++) {
    dev_holder.objdev[i].ref = 0;
  }
}

int getorcreatedevice(struct vfs_inode *ip) {
  acquire(&dev_holder.lock);
  int emptydevice = -1;
  for (int i = 0; i < NLOOPDEVS; i++) {
    if (dev_holder.loopdevs[i].ref == 0 && emptydevice == -1) {
      emptydevice = i;
    } else if (dev_holder.loopdevs[i].ip == ip) {
      dev_holder.loopdevs[i].ref++;
      release(&dev_holder.lock);
      return LOOP_DEVICE_TO_DEV(i);
    }
  }

  if (emptydevice == -1) {
    release(&dev_holder.lock);
    return -1;
  }

  dev_holder.loopdevs[emptydevice].ref = 1;
  dev_holder.loopdevs[emptydevice].ip = ip->i_op.idup(ip);
  release(&dev_holder.lock);
  fsinit(LOOP_DEVICE_TO_DEV(emptydevice));
  return LOOP_DEVICE_TO_DEV(emptydevice);
}

int createobjdevice() {
  acquire(&dev_holder.lock);
  int emptydevice = -1;
  for (int i = 0; i < NOBJDEVS; i++) {
    if (dev_holder.objdev[i].ref == 0 && emptydevice == -1) {
      emptydevice = i;
    }
  }

  if (emptydevice == -1) {
    cprintf("Not available obj device\n");
    release(&dev_holder.lock);
    return -1;
  }

  dev_holder.objdev[emptydevice].ref = 1;
  release(&dev_holder.lock);

  obj_fs_init_dev(OBJ_TO_DEV(emptydevice));
  return OBJ_TO_DEV(emptydevice);
}

struct obj_device *objdeviceget(uint dev) {
  if (!IS_OBJ_DEVICE(dev)) {
    panic("not obj device");
  }

  dev = DEV_TO_OBJ_DEVICE(dev);
  acquire(&dev_holder.lock);
  dev_holder.objdev[dev].ref++;
  release(&dev_holder.lock);

  return &dev_holder.objdev[dev];
}

void deviceget(uint dev) {
  if (IS_LOOP_DEVICE(dev)) {
    dev = DEV_TO_LOOP_DEVICE(dev);
    acquire(&dev_holder.lock);
    dev_holder.loopdevs[dev].ref++;
    release(&dev_holder.lock);
  } else if (IS_OBJ_DEVICE(dev)) {
    dev = DEV_TO_OBJ_DEVICE(dev);
    acquire(&dev_holder.lock);
    dev_holder.objdev[dev].ref++;
    release(&dev_holder.lock);
  }
}

void deviceput(uint dev) {
  if (IS_LOOP_DEVICE(dev)) {
    dev = DEV_TO_LOOP_DEVICE(dev);
    acquire(&dev_holder.lock);
    if (dev_holder.loopdevs[dev].ref == 1) {
      release(&dev_holder.lock);

      dev_holder.loopdevs[dev].ip->i_op.iput(dev_holder.loopdevs[dev].ip);
      buf_cache_invalidate_blocks(LOOP_DEVICE_TO_DEV(dev));

      acquire(&dev_holder.lock);
      dev_holder.loopdevs[dev].ip = 0;
    }
    dev_holder.loopdevs[dev].ref--;
    release(&dev_holder.lock);
  } else if (IS_OBJ_DEVICE(dev)) {
    dev = DEV_TO_OBJ_DEVICE(dev);
    acquire(&dev_holder.lock);
    dev_holder.objdev[dev].ref--;
    if (dev_holder.objdev[dev].ref == 0) {
      release(&dev_holder.lock);
      buf_cache_invalidate_blocks(dev);
      acquire(&dev_holder.lock);
    }
    release(&dev_holder.lock);
  }
}

struct vfs_inode *getinodefordevice(uint dev) {
  if (!IS_LOOP_DEVICE(dev)) {
    return 0;
  }

  dev = DEV_TO_LOOP_DEVICE(dev);

  if (dev_holder.loopdevs[dev].ref == 0) {
    return 0;
  }

  return dev_holder.loopdevs[dev].ip;
}

struct vfs_superblock *getsuperblock(uint dev) {
  if (IS_LOOP_DEVICE(dev)) {
    uint loopdev = DEV_TO_LOOP_DEVICE(dev);
    if (loopdev >= NLOOPDEVS) {
      panic("could not find superblock for device: device number to high");
    }
    if (dev_holder.loopdevs[loopdev].ref == 0) {
      panic("could not find superblock for device: device ref count is 0");
    } else {
      return &dev_holder.loopdevs[loopdev].sb.vfs_sb;
    }
  } else if (IS_OBJ_DEVICE(dev)) {
    uint objdev = DEV_TO_OBJ_DEVICE(dev);
    if (objdev >= NOBJDEVS) {
      panic("could not find obj superblock for device: device number to high");
    }
    if (dev_holder.objdev[objdev].ref == 0) {
      panic("could not find obj superblock for device: device ref count is 0");
    } else {
      return &dev_holder.objdev[objdev].sb.vfs_sb;
    }
  } else if (dev < NIDEDEVS) {
    return &dev_holder.idesb[dev].vfs_sb;
  } else {
    cprintf("could not find superblock for device, dev: %d\n", dev);
    panic("could not find superblock for device");
  }
}

int doesbackdevice(struct vfs_inode *ip) {
  acquire(&dev_holder.lock);
  for (int i = 0; i < NLOOPDEVS; i++) {
    if (dev_holder.loopdevs[i].ip == ip) {
      release(&dev_holder.lock);
      return 1;
    }
  }
  release(&dev_holder.lock);
  return 0;
}
