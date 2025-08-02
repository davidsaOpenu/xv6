#ifndef XV6_DEVICE_DEVICE_H
#define XV6_DEVICE_DEVICE_H

#include "spinlock.h"

#define MAX_LOOP_DEVS_NUM (10)
#define MAX_IDE_DEVS_NUM (1)  // currently only one ide device is supported
#define MAX_OBJ_DEVS_NUM (3)

#define NMAXDEVS (MAX_LOOP_DEVS_NUM + MAX_IDE_DEVS_NUM + MAX_OBJ_DEVS_NUM)

struct device;

enum device_type {
  DEVICE_TYPE_NONE = 0,
  DEVICE_TYPE_IDE,
  DEVICE_TYPE_LOOP,
  DEVICE_TYPE_OBJ,

  DEVICE_TYPE_MAX
};

struct device_ops {
  void (*destroy)(struct device* dev);
};

struct device {
  int ref;
  int id;
  enum device_type type;
  void* private;
  const struct device_ops* ops;
};

struct dev_holder_s {
  struct spinlock lock;  // protects loopdevs
  struct device devs[NMAXDEVS];
  uint devs_count[DEVICE_TYPE_MAX];
};

extern struct dev_holder_s dev_holder;

extern const struct device_ops default_device_ops;

struct device* _get_new_device(enum device_type type);

void* dev_private(struct device* dev);

void deviceput(struct device*);
void deviceget(struct device*);
void devinit(void);

#endif /* XV6_DEVICE_DEVICE_H */
