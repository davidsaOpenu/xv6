#ifndef XV6_DEVICE_BIO_H
#define XV6_DEVICE_BIO_H

#include "buf.h"
#include "device.h"

struct buf* bread(const struct device* const, uint);
void bwrite(struct buf*);

#endif  // XV6_DEVICE_BIO_H
