#include "fs.h"

#include "native_fs.h"
#include "obj_fs.h"

void fsinit() {
  native_iinit();
  obj_iinit();
}
