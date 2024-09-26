#ifndef XV6_DEVICE_OBJ_CACHE_H
#define XV6_DEVICE_OBJ_CACHE_H

#include "device.h"
#include "types.h"

struct device* create_ide_device(uint ide_port);
struct device* get_ide_device(uint ide_port);

#endif  // XV6_DEVICE_OBJ_CACHE_H
