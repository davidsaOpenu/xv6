#ifndef XV6_DEVICE_IDE_H
#define XV6_DEVICE_IDE_H

#include "buf.h"
void ideinit(void);
void ideintr(void);
void iderw(struct buf*);

#endif  // XV6_DEVICE_IDE_H
