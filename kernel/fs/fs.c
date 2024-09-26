#include "fs.h"

#include "native_fs.h"
#include "obj_fs.h"
#include "fs/vfs_file.h"

void fsinit() {
  vfs_fileinit();   // file table
  native_iinit();
  obj_iinit();
}
