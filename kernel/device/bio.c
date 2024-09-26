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
#include "bio.h"

#include "buf.h"
#include "buf_cache.h"
#include "cgroup.h"
#include "defs.h"
#include "fs/vfs_file.h"
#include "fs/vfs_fs.h"
#include "ide.h"
#include "kvector.h"
#include "proc.h"

static void devicerw(struct vfs_inode *const vfs_inode, struct buf *const b) {
  if ((b->flags & B_DIRTY) == 0) {
    vector read_result_vector;
    read_result_vector = newvector(BSIZE, 1);
    vfs_inode->i_op->readi(vfs_inode, BSIZE * b->id.blockno, BSIZE,
                           &read_result_vector);
    memmove_from_vector((char *)b->data, read_result_vector, 0, BSIZE);
    // vectormemcmp("devicerw", read_result_vector, 0, (char *) b->data, BSIZE);
    freevector(&read_result_vector);
  } else {
    vfs_inode->i_op->writei(vfs_inode, (char *)b->data, BSIZE * b->id.blockno,
                            BSIZE);
  }
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
}

static void brw(struct buf *const b) {
  struct vfs_inode *inode_of_loop_dev;
  // Support for loop devices
  if ((inode_of_loop_dev = getinodefordevice(b->dev)) != 0) {
    devicerw(inode_of_loop_dev, b);
  } else {
    iderw(b);
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(const struct device *const dev, const uint blockno) {
  struct buf *b;
  union buf_id id = {.blockno = blockno};

  b = buf_cache_get(dev, &id, 0);
  if ((b->flags & B_VALID) == 0) {
    brw(b);
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *const b) {
  if (!holdingsleep(&b->lock)) panic("bwrite");
  b->flags |= B_DIRTY;
  brw(b);
}

// PAGEBREAK!
//  Blank page.
