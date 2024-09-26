#include "device.h"

struct device* get_ide_device(const uint ide_port) {
  struct device* found = NULL;
  acquire(&dev_holder.lock);
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    if (dev->private != NULL && dev->private == (void*)ide_port &&
        dev->type == DEVICE_TYPE_IDE) {
      dev->ref++;
      found = dev;
      goto end;
    }
  }

end:
  release(&dev_holder.lock);
  return found;
}

struct device* create_ide_device(const uint ide_port) {
  acquire(&dev_holder.lock);
  struct device* dev = _get_new_device(DEVICE_TYPE_IDE);

  if (dev == NULL) {
    goto end;
  }

  dev->private = (void*)ide_port;
  dev->ops = &default_device_ops;

end:
  release(&dev_holder.lock);
  return dev;
}
