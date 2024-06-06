// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include "buf.h"
#include "cgroup.h"
#include "defs.h"
#include "file.h"
#include "fs.h"
#include "kvector.h"
#include "proc.h"
#include "vfs_file.h"
#include "vfs_fs.h"

void devicerw(struct inode *device, struct buf *b) {
  if ((b->flags & B_DIRTY) == 0) {
    vector read_result_vector;
    read_result_vector = newvector(BSIZE, 1);
    device->vfs_inode.i_op.readi(&device->vfs_inode, BSIZE * b->id.blockno,
                                 BSIZE, &read_result_vector);
    memmove_from_vector((char *)b->data, read_result_vector, 0, BSIZE);
    // vectormemcmp("devicerw", read_result_vector, 0, (char *) b->data, BSIZE);
    freevector(&read_result_vector);
  } else {
    device->vfs_inode.i_op.writei(&device->vfs_inode, (char *)b->data,
                                  BSIZE * b->id.blockno, BSIZE);
  }
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
}

void brw(struct buf *b) {
  struct vfs_inode *device;
  if ((device = getinodefordevice(b->dev)) != 0) {
    struct inode *i_device = container_of(device, struct inode, vfs_inode);

    devicerw(i_device, b);
  } else {
    iderw(b);
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;
  union buf_id id = {.blockno = blockno};

  b = buf_cache_get(dev, &id, 0);
  if ((b->flags & B_VALID) == 0) {
    brw(b);
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("bwrite");
  b->flags |= B_DIRTY;
  brw(b);
}

// PAGEBREAK!
//  Blank page.
