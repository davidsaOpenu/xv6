#ifndef XV6_BUF_H
#define XV6_BUF_H

#include "../common/fs.h"
#include "../common/types.h"
#include "sleeplock.h"

struct buf {
  int flags;
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev;  // LRU cache list
  struct buf *next;
  struct buf *qnext;  // disk queue
  struct cgroup *cgroup;
  uchar data[BSIZE];
};
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

#endif /* XV6_BUF_H */
