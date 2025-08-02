#include "device.h"

#include "buf_cache.h"
#include "defs.h"
#include "ide.h"
#include "ide_device.h"
#include "obj_disk.h"
#include "param.h"
#include "sleeplock.h"
#include "types.h"

struct dev_holder_s dev_holder = {0};

void* dev_private(struct device* dev) { return dev->private; }

void devinit() {
  int i = 0;
  initlock(&dev_holder.lock, "dev_list");
  for (struct device* dev = dev_holder.devs; dev < &dev_holder.devs[NMAXDEVS];
       dev++) {
    dev->id = i++;
    dev->ref = 0;
    dev->type = DEVICE_TYPE_NONE;
    dev->ops = NULL;
    dev->private = NULL;
  }
  memset(dev_holder.devs_count, 0, sizeof(dev_holder.devs_count));

  buf_cache_init();  // buffer cache
  ideinit();         // disk

  // Register initial IDE device we booted from
  if (create_ide_device(ROOTDEV) == NULL) panic("Failed to mount /!");
}

static void destroy_dev_default(struct device* const dev) {
  dev->private = NULL;
}

const struct device_ops default_device_ops = {
    .destroy = &destroy_dev_default,
};

static const int dev_to_max_count[] = {
    0,                  // DEVICE_TYPE_NONE
    MAX_IDE_DEVS_NUM,   // DEVICE_TYPE_IDE
    MAX_LOOP_DEVS_NUM,  // DEVICE_TYPE_LOOP
    MAX_OBJ_DEVS_NUM,   // DEVICE_TYPE_OBJ
};

// Must hold dev_holder.lock.
struct device* _get_new_device(const enum device_type type) {
  struct device* dev = NULL;

  XV6_ASSERT(type < DEVICE_TYPE_MAX);
  XV6_ASSERT(type > DEVICE_TYPE_NONE);
  if (dev_holder.devs_count[type] >= dev_to_max_count[type]) {
    return NULL;
  }

  for (struct device* current_dev = dev_holder.devs;
       current_dev < &dev_holder.devs[NMAXDEVS]; current_dev++) {
    if (current_dev->ref == 0 && current_dev->type == DEVICE_TYPE_NONE) {
      dev = current_dev;
      break;
    }
  }
  if (dev == NULL) {
    return NULL;
  }
  dev->ref = 1;
  dev->type = type;
  dev->private = NULL;
  dev->ops = &default_device_ops;

  dev_holder.devs_count[type]++;

  return dev;
}

void deviceget(struct device* const dev) {
  XV6_ASSERT(dev->ref > 0);

  acquire(&dev_holder.lock);
  dev->ref++;
  release(&dev_holder.lock);
}

void deviceput(struct device* const d) {
  XV6_ASSERT(d->type > DEVICE_TYPE_NONE);
  XV6_ASSERT(d->type < DEVICE_TYPE_MAX);
  XV6_ASSERT(d->ref > 0);

  acquire(&dev_holder.lock);
  if (d->ref == 1) {
    release(&dev_holder.lock);

    // now we can destroy the device.
    d->ops->destroy(d);

    // update counter
    XV6_ASSERT(dev_holder.devs_count[d->type] > 0);
    dev_holder.devs_count[d->type]--;

    // remove fields
    d->type = DEVICE_TYPE_NONE;
    d->private = NULL;
    d->ops = NULL;

    acquire(&dev_holder.lock);
  }
  d->ref--;
  release(&dev_holder.lock);
}
