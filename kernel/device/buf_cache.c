#include "buf_cache.h"

#include "buf.h"
#include "cgroup.h"
#include "param.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;

  // Whether buffers are cached upon release, or invalidated immediately.
  // Used mostly for performance measurements.
  uint is_cache_enabled;
} bufs_cache;

void buf_cache_init(void) {
  struct buf *b;

  initlock(&bufs_cache.lock, "bufs_cache");

  // PAGEBREAK!
  //  Create linked list of buffers
  bufs_cache.head.prev = &bufs_cache.head;
  bufs_cache.head.next = &bufs_cache.head;
  for (b = bufs_cache.buf; b < bufs_cache.buf + NBUF; b++) {
    b->next = bufs_cache.head.next;
    b->prev = &bufs_cache.head;
    initsleeplock(&b->lock, "buffer");
    bufs_cache.head.next->prev = b;
    bufs_cache.head.next = b;
  }

  bufs_cache.is_cache_enabled = 1;
}

void buf_cache_invalidate_blocks(const struct device *const dev) {
  acquire(&bufs_cache.lock);
  struct buf *b;
  for (b = bufs_cache.head.next; b != &bufs_cache.head; b = b->next) {
    if (b->dev == dev) {
      b->flags &= ~(B_VALID | B_DIRTY);
    }
  }
  release(&bufs_cache.lock);
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
struct buf *buf_cache_get(const struct device *const dev,
                          const union buf_id *id, const uint alloc_flags) {
  struct buf *b;
  struct cgroup *cg = proc_get_cgroup();

  acquire(&bufs_cache.lock);

  // Is the block already cached?
  for (b = bufs_cache.head.next; b != &bufs_cache.head; b = b->next) {
    if (b->dev == dev && (0 == memcmp(&(b->id), id, sizeof(*id)))) {
      b->refcnt++;
      release(&bufs_cache.lock);
      acquiresleep(&b->lock);
      cgroup_mem_stat_pgfault_incr(cg);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.
  // Even if refcnt==0, B_DIRTY indicates a buffer is in use
  // because log.c has modified it but not yet committed it.
  for (b = bufs_cache.head.prev; b != &bufs_cache.head; b = b->prev) {
    if (b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev;
      b->id = *id;
      b->flags = 0;
      b->alloc_flags = alloc_flags;
      b->cgroup = 0;
      b->refcnt = 1;
      release(&bufs_cache.lock);
      acquiresleep(&b->lock);
      cgroup_mem_stat_pgmajfault_incr(cg);
      return b;
    }
  }
  panic("buf_cache_get: no buffers");
}

// Release a locked buffer.
void buf_cache_release(struct buf *const b) {
  uint insert_first = 0;
  if (!holdingsleep(&b->lock)) panic("buf_cache_release");

  releasesleep(&b->lock);

  acquire(&bufs_cache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    if (!bufs_cache.is_cache_enabled) {
      insert_first = 0;
      // When the cache is disabled and the buffer is not used anymore,
      // use the buf only as a memory and therefore invalidate it.
      if (!(b->flags & B_DIRTY)) {
        b->flags &= ~B_VALID;
      }
    } else {
      if (b->alloc_flags & BUF_ALLOC_NO_CACHE) {
        // This buffer does not have priority.
        insert_first = 0;
      } else {
        insert_first = 1;
      }
    }

    // Remove the buf from the list and re-insert it according to its type
    b->next->prev = b->prev;
    b->prev->next = b->next;
    if (insert_first) {
      // Move to the head of the MRU list.
      b->next = bufs_cache.head.next;
      b->prev = &bufs_cache.head;
      bufs_cache.head.next->prev = b;
      bufs_cache.head.next = b;
    } else {
      // Move to the tail of the MRU list.
      b->prev = bufs_cache.head.prev;
      b->next = &bufs_cache.head;
      bufs_cache.head.prev->next = b;
      bufs_cache.head.prev = b;
    }
  }

  release(&bufs_cache.lock);
}

uint buf_cache_is_cache_enabled(void) {
  uint is_cache_enabled = 0;

  acquire(&bufs_cache.lock);
  is_cache_enabled = bufs_cache.is_cache_enabled;
  release(&bufs_cache.lock);

  return is_cache_enabled;
}

void buf_cache_enable_cache(void) {
  acquire(&bufs_cache.lock);

  bufs_cache.is_cache_enabled = 1;

  release(&bufs_cache.lock);
}

void buf_cache_disable_cache(void) {
  struct buf *b;
  acquire(&bufs_cache.lock);

  if (bufs_cache.is_cache_enabled) {
    // Invalidate all unused bufs
    for (b = bufs_cache.head.next; b != &bufs_cache.head; b = b->next) {
      if (b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
        b->flags &= ~B_VALID;
      }
    }
    bufs_cache.is_cache_enabled = 0;
  }

  release(&bufs_cache.lock);
}
