#include "fs.h"

#include "fs/vfs_file.h"
#include "native_fs.h"
#include "obj_fs.h"

void fsinit() {
  vfs_fileinit();  // file table
  native_iinit();
  obj_iinit();
}
