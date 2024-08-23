#ifndef XV6_FILE_H
#define XV6_FILE_H

#include "../common/fs.h"
#include "../common/param.h"
#include "sleeplock.h"
#include "vfs_file.h"

// in-memory copy of an inode
// that also keeps one to one correspondence between a physical inode and it's
// vfs_inode counterpart
struct inode {
  uint size;
  uint addrs[NDIRECT + 1];
  struct vfs_inode vfs_inode;
};

#endif /* XV6_FILE_H */
