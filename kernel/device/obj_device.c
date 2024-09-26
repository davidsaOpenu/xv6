#include "obj_device.h"

#include "obj_disk.h"

struct device* create_obj_device() {
  acquire(&dev_holder.lock);
  struct device* dev = _get_new_device(DEVICE_TYPE_OBJ);
  init_obj_device(dev);
  release(&dev_holder.lock);
  return dev;
}
