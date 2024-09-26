#ifndef XV6_DEVICE_BUF_H
#define XV6_DEVICE_BUF_H

#include "fs/obj_fs.h"
#include "fsdefs.h"
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
  const struct device *dev;
  union buf_id id;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev;  // LRU cache list
  struct buf *next;
  struct buf *qnext;  // disk queue
  struct cgroup *cgroup;
  uchar data[BUF_DATA_SIZE];
};

enum B_FLAGS_SHIFT {
  B_VALID_SHIFT = 1,
  B_DIRTY_SHIFT = 2,
};

#define B_VALID (1 << B_VALID_SHIFT)  // buffer has been read from disk
#define B_DIRTY (1 << B_DIRTY_SHIFT)  // buffer needs to be written to disk

#endif /* XV6_DEVICE_BUF_H */
