#ifndef XV6_DEVICE_OBJ_CACHE_H
#define XV6_DEVICE_OBJ_CACHE_H

/**
 * Objfs cache module.
 * Allocate and cache buffers for objects data.
 *
 * NOTE: All exported functions of this module assume the inode of the object
 *       param is locked when called.
 *
 * Implementation details:
 * ~~~~~~~~~~~~~~~~~~~~~~~
 * This module uses the "struct buf" and "buf_cache" caching module as its
 * internal implementation. It splits objects to buffers, and manages all
 * buffers of the same objet, such that deadlocks are avoided.
 *
 * Big objects might take a lot of cache space. When the usage is of only a
 * small part of a big object, following cache buffer allocations will cause
 * invalidation of much more useful cache buffers instead of using the now
 * unused cache buffers of the big object. Therefore, cache only buffers around
 * the requested data area, and use the rest of the buffers as temporal memory.
 */

#include "buf.h"
#include "kvector.h"
#include "types.h"

/* The number of blocks to cache around the requested contiguous data area. */
#define OBJ_CACHE_BLOCKS_PADDING (3)

#define OFFSET_ROUND_DOWN(offset) ((offset) & ~(BUF_DATA_SIZE - 1))
#define OFFSET_ROUND_UP(offset) \
  (OFFSET_ROUND_DOWN((offset) + BUF_DATA_SIZE - 1))
#define OFFSET_TO_BLOCKNO(offset) ((offset) / BUF_DATA_SIZE)
#define SIZE_TO_NUM_OF_BUFS(size) (OFFSET_ROUND_UP(size) / BUF_DATA_SIZE)

void obj_cache_init();

/* NOTE: The following functions of this module assume the inode of the object
 *       param is locked when called. */
uint obj_cache_add(struct device* dev, const char* name, const void* data,
                   uint size);
uint obj_cache_write(struct device* dev, const char* name, const void* data,
                     uint size, uint offset, uint prev_obj_size);
uint obj_cache_read(struct device* dev, const char* name, vector* dst,
                    uint size, uint offset, uint obj_size);
uint obj_cache_delete(struct device* dev, const char* name, uint obj_size);

/**
 * The following methods provides statistics about the cache layer. They can
 * used by program to show performance of the file system or to try and
 * optimize their run flow.
 */

uint objects_cache_hits();
uint objects_cache_misses();
uint cache_max_object_size();

#endif /* XV6_DEVICE_OBJ_CACHE_H */
