#ifndef XV6_BUF_H
#define XV6_BUF_H

#include "fs.h"
#include "obj_fs.h"
#include "sleeplock.h"
#include "types.h"

#define BUF_DATA_SIZE BSIZE

// Indicate buffer should not be cached after released
#define BUF_ALLOC_NO_CACHE (0x1)

union buf_id {
  // nativefs id
  uint blockno;

  // objfs id
  struct {
    char object_name[MAX_OBJECT_NAME_LENGTH];
    uint blockno;
  } obj_id;
};

struct buf {
  uint flags;
  uint alloc_flags;
  uint dev;
  // TODO(SM): Remove the next member when nativefs uses the new buffers cache
  uint blockno;
  union buf_id id;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev;  // LRU cache list
  struct buf *next;
  struct buf *qnext;  // disk queue
  struct cgroup *cgroup;
  uchar data[BUF_DATA_SIZE];
};
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

#endif /* XV6_BUF_H */
