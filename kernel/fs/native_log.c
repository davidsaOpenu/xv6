#include "cgroup.h"
#include "defs.h"
#include "device/bio.h"
#include "device/buf.h"
#include "device/buf_cache.h"
#include "device/device.h"
#include "native_fs.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "types.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding;  // how many FS sys calls are executing.
  int committing;   // in commit(), please wait.
  struct device *dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void initlog(struct vfs_superblock *vfs_sb) {
  XV6_ASSERT(vfs_sb->private != NULL);

  struct native_superblock_private *sb = sb_private(vfs_sb);
  if (sizeof(struct logheader) >= BSIZE) panic("initlog: too big logheader");

  struct native_superblock_private *sbp = sb_private(vfs_sb);
  deviceget(sbp->dev);

  initlock(&log.lock, "log");
  log.start = sb->sb.logstart;
  log.size = sb->sb.nlog;
  log.dev = sbp->dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void install_trans(void) {
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start + tail + 1);  // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);    // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);                            // write dst to disk
    cgroup_mem_stat_file_dirty_decr(dbuf->cgroup);
    cgroup_mem_stat_file_dirty_aggregated_incr(dbuf->cgroup);
    buf_cache_release(lbuf);
    buf_cache_release(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void read_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)(buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  buf_cache_release(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void write_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *)(buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  buf_cache_release(buf);
}

static void recover_from_log(void) {
  read_head();
  install_trans();  // if committed, copy from log to disk
  log.lh.n = 0;
  write_head();  // clear the log
}

// called at the start of each FS system call.
void begin_op(void) {
  acquire(&log.lock);
  while (1) {
    if (log.committing) {
      sleep(&log, &log.lock);
    } else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE) {
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void end_op(void) {
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if (log.committing) panic("log.committing");
  if (log.outstanding == 0) {
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if (do_commit) {
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void write_log(void) {
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start + tail + 1);  // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]);  // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    buf_cache_release(from);
    buf_cache_release(to);
  }
}

static void commit() {
  if (log.lh.n > 0) {
    write_log();      // Write modified blocks from cache to log
    write_head();     // Write header to disk -- the real commit
    install_trans();  // Now install writes to home locations
    log.lh.n = 0;
    write_head();  // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   buf_cache_release(bp)
void log_write(struct buf *b) {
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1) panic("log_write outside of trans");

  if (b->dev->type == DEVICE_TYPE_LOOP) {
    // disable journaling for loop devices.
    bwrite(b);
    return;
  }

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->id.blockno)  // log absorbtion
      break;
  }
  log.lh.block[i] = b->id.blockno;
  if (i == log.lh.n) {
    log.lh.n++;
    b->cgroup = proc_get_cgroup();
    cgroup_mem_stat_file_dirty_incr(b->cgroup);
  }
  b->flags |= B_DIRTY;  // prevent eviction
  release(&log.lock);
}
