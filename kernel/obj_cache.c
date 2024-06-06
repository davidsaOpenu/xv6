#include "obj_cache.h"

#include "obj_disk.h"
#include "proc.h"

uint hits;
uint misses;

struct bufs_alloc_hint {
  uint start_index;
  uint count;
  uint flags;
};

static uint obj_cache_are_bufs_valid(vector bufs) {
  struct buf *curr_buf;

  for (uint index = 0; index < bufs.vectorsize; index++) {
    memmove_from_vector((char *)&curr_buf, bufs, index, 1);
    if (!(curr_buf->flags & B_VALID)) {
      return 0;
    }
  }

  return 1;
}

static vector obj_cache_get_bufs(uint dev, const char *name, uint start,
                                 uint count, struct bufs_alloc_hint *hints) {
  struct buf *curr_buf;
  vector bufs = newvector(count, sizeof(struct buf *));
  union buf_id id;
  uint alloc_flags;

  strncpy(id.obj_id.object_name, name, MAX_OBJECT_NAME_LENGTH);
  id.obj_id.object_name[MAX_OBJECT_NAME_LENGTH - 1] = '\0';
  for (uint index = 0; index < count; index++) {
    uint blockno = start + index;
    if ((0 == hints) || (hints->count == 0) || (blockno < hints->start_index)) {
      alloc_flags = 0;
    } else {
      alloc_flags = hints->flags;
      // If this is the last block in the current hints, move to the
      // next block hints
      if (blockno == (hints->start_index + hints->count - 1)) {
        hints++;
      }
    }
    id.obj_id.blockno = blockno;
    curr_buf = buf_cache_get(dev, &id, alloc_flags);
    memmove_into_vector_elements(bufs, index, (char *)&curr_buf, 1);
  }

  return bufs;
}

static void obj_cache_release_bufs(vector bufs) {
  struct buf *curr_buf;

  for (uint index = 0; index < bufs.vectorsize; index++) {
    memmove_from_vector((char *)&curr_buf, bufs, index, 1);
    buf_cache_release(curr_buf);
  }

  freevector(&bufs);
}

static void obj_cache_copy_to_bufs(vector bufs, const void *data, uint size,
                                   uint offset) {
  struct buf *curr_buf;
  uint first_block = OFFSET_TO_BLOCKNO(offset);
  uint last_block = OFFSET_TO_BLOCKNO(offset + size - 1);

  uint copied_bytes = 0;
  for (uint curr_block = first_block; curr_block <= last_block; curr_block++) {
    uint in_block_offset = 0;
    uint block_size = BUF_DATA_SIZE;
    if (curr_block == first_block) {
      in_block_offset = offset - OFFSET_ROUND_DOWN(offset);
      block_size = BUF_DATA_SIZE - in_block_offset;
    }
    if (curr_block == last_block) {
      block_size = min(size - copied_bytes, BUF_DATA_SIZE);
    }

    memmove_from_vector((char *)&curr_buf, bufs, curr_block, 1);
    memmove(curr_buf->data + in_block_offset, ((char *)data) + copied_bytes,
            block_size);
    curr_buf->flags |= B_VALID | B_DIRTY;
    copied_bytes += block_size;
  }
}

static void obj_cache_copy_from_bufs(vector bufs, uint size, uint offset,
                                     vector dst) {
  struct buf *curr_buf;
  uint first_block = OFFSET_TO_BLOCKNO(offset);
  uint last_block = OFFSET_TO_BLOCKNO(offset + size - 1);

  uint copied_bytes = 0;
  for (uint curr_block = first_block; curr_block <= last_block; curr_block++) {
    uint in_block_offset = 0;
    uint block_size = BUF_DATA_SIZE;
    if (curr_block == first_block) {
      in_block_offset = offset - OFFSET_ROUND_DOWN(offset);
      block_size = BUF_DATA_SIZE - in_block_offset;
    }
    if (curr_block == last_block) {
      block_size = min(size - copied_bytes, BUF_DATA_SIZE);
    }

    memmove_from_vector((char *)&curr_buf, bufs, curr_block, 1);
    memmove_into_vector_bytes(dst, copied_bytes,
                              (char *)(curr_buf->data + in_block_offset),
                              block_size);
    copied_bytes += block_size;
  }
}

static uint validate_bufs(uint dev, const char *name, uint obj_size,
                          vector obj_bufs, uint size, uint offset) {
  uint err = NO_ERR;
  struct buf *curr_buf;
  uint found_invalid = 0;

  for (uint index = 0; index < size; index++) {
    memmove_from_vector((char *)&curr_buf, obj_bufs, offset + index, 1);
    if (!(curr_buf->flags & B_VALID)) {
      found_invalid = 1;
    }
  }

  if (found_invalid) {
    // Read the object from disk
    err = get_object(dev, name, obj_bufs);
    if (NO_ERR != err) {
      return err;
    }
  }

  return NO_ERR;
}

static void obj_cache_invalidate_bufs(vector bufs) {
  struct buf *curr_buf;

  for (uint index = 0; index < bufs.vectorsize; index++) {
    memmove_from_vector((char *)&curr_buf, bufs, index, 1);
    curr_buf->flags &= ~B_VALID;
  }
}

// Set hints to cache only a contiguous area inside a buffer (and pad around)
// Don't cache the data before and after.
// NOTE: The 'alloc_hints' param should be in size of at least 3.
static void obj_cache_set_contiguous_area_hints(
    struct bufs_alloc_hint alloc_hints[], uint size, uint offset,
    uint obj_size) {
  uint hints_index = 0;
  uint first_buf_index;
  uint bufs_count;

  // Set hints for buffers before contiguous area (if there is such)
  if (OFFSET_TO_BLOCKNO(offset) > OBJ_CACHE_BLOCKS_PADDING) {
    alloc_hints[hints_index++] = (struct bufs_alloc_hint){
        .start_index = 0,
        .count = OFFSET_TO_BLOCKNO(offset) - OBJ_CACHE_BLOCKS_PADDING,
        .flags = BUF_ALLOC_NO_CACHE};
  }

  // Set hints for buffers after contiguous data (if there is such)
  if ((OFFSET_TO_BLOCKNO(offset + size - 1) + OBJ_CACHE_BLOCKS_PADDING) <
      OFFSET_TO_BLOCKNO(obj_size - 1)) {
    first_buf_index =
        OFFSET_TO_BLOCKNO(offset + size - 1) + OBJ_CACHE_BLOCKS_PADDING + 1;
    bufs_count = OFFSET_TO_BLOCKNO(obj_size - 1) - first_buf_index + 1;
    alloc_hints[hints_index++] =
        (struct bufs_alloc_hint){.start_index = first_buf_index,
                                 .count = bufs_count,
                                 .flags = BUF_ALLOC_NO_CACHE};
  }

  // Indicate no more hints.
  alloc_hints[hints_index++] = (struct bufs_alloc_hint){.count = 0};
}

uint obj_cache_add(uint dev, const char *name, const void *data, uint size) {
  uint err = NO_ERR;
  vector obj_bufs = {0};
  struct bufs_alloc_hint alloc_hints[3];

  if (size > 0) {
    // Don't cache the whole object, but just the requested data and padding
    obj_cache_set_contiguous_area_hints(
        alloc_hints, OBJ_CACHE_BLOCKS_PADDING * BUF_DATA_SIZE, 0, size);

    obj_bufs = obj_cache_get_bufs(dev, name, 0, OFFSET_TO_BLOCKNO(size - 1) + 1,
                                  alloc_hints);
    obj_cache_copy_to_bufs(obj_bufs, data, size, 0);
  }

  err = add_object(dev, name, obj_bufs, size);
  if (NO_ERR != err) {
    goto clean;
  }

clean:
  if (size > 0) {
    obj_cache_release_bufs(obj_bufs);
  }

  return NO_ERR;
}

uint obj_cache_write(uint dev, const char *name, const void *data, uint size,
                     uint offset, uint prev_obj_size) {
  uint err = NO_ERR;
  vector obj_bufs = {0};
  uint new_obj_size =
      max(offset + size, prev_obj_size);  // NOLINT(build/include_what_you_use)
  uint first_buf_index;
  uint bufs_count;
  struct bufs_alloc_hint alloc_hints[3];

  // Don't cache the whole object, but just the requested data and padding
  obj_cache_set_contiguous_area_hints(alloc_hints, size, offset, new_obj_size);

  obj_bufs = obj_cache_get_bufs(
      dev, name, 0, OFFSET_TO_BLOCKNO(new_obj_size - 1) + 1, alloc_hints);

  // If we don't write the whole object we need to read the rest of the content
  // (from either disk or cache) as writes are done on entire objects
  if (offset > 0) {
    err = validate_bufs(dev, name, prev_obj_size, obj_bufs,
                        OFFSET_TO_BLOCKNO(offset - 1) + 1, 0);
    if (NO_ERR != err) {
      goto clean;
    }
  }

  if ((offset + size) < prev_obj_size) {
    first_buf_index = OFFSET_TO_BLOCKNO(offset + size);
    bufs_count = OFFSET_TO_BLOCKNO(prev_obj_size - 1) - first_buf_index + 1;
    err = validate_bufs(dev, name, prev_obj_size, obj_bufs, bufs_count,
                        first_buf_index);
    if (NO_ERR != err) {
      goto clean;
    }
  }

  // Copy the new data to bufs
  obj_cache_copy_to_bufs(obj_bufs, data, size, offset);

  err = write_object(dev, name, obj_bufs, new_obj_size);
  if (NO_ERR != err) {
    goto clean;
  }

clean:
  obj_cache_release_bufs(obj_bufs);

  return err;
}

uint obj_cache_read(uint dev, const char *name, vector *dst, uint size,
                    uint offset, uint obj_size) {
  uint err = NO_ERR;
  vector obj_bufs = {0};
  uint start_block = OFFSET_TO_BLOCKNO(offset);
  uint end_block = OFFSET_TO_BLOCKNO(offset + size - 1);
  struct bufs_alloc_hint alloc_hints[3];

  // Try to read the object directly from cache
  obj_bufs = obj_cache_get_bufs(dev, name, start_block,
                                end_block - start_block + 1, 0);
  if (obj_cache_are_bufs_valid(obj_bufs)) {
    obj_cache_copy_from_bufs(obj_bufs, size, offset - OFFSET_ROUND_DOWN(offset),
                             *dst);
  } else {
    // The data we want to read is not entirely on cache, we need to read the
    // whole object directly from disk
    obj_cache_release_bufs(obj_bufs);

    // Don't cache the whole object, but just the requested data and padding
    obj_cache_set_contiguous_area_hints(alloc_hints, size, offset, obj_size);

    obj_bufs = obj_cache_get_bufs(
        dev, name, 0, OFFSET_TO_BLOCKNO(obj_size - 1) + 1, alloc_hints);
    err = get_object(dev, name, obj_bufs);
    if (NO_ERR != err) {
      goto clean;
    }

    obj_cache_copy_from_bufs(obj_bufs, size, offset, *dst);
  }

clean:
  obj_cache_release_bufs(obj_bufs);
  return err;
}

uint obj_cache_delete(uint dev, const char *name, uint obj_size) {
  uint err = NO_ERR;

  if (obj_size > 0) {
    vector obj_bufs = {0};
    uint bufs_count = OFFSET_TO_BLOCKNO(obj_size - 1) + 1;
    struct bufs_alloc_hint alloc_hints[] = {
        {.start_index = 0, .count = bufs_count, .flags = BUF_ALLOC_NO_CACHE},
        {.count = 0}};

    obj_bufs = obj_cache_get_bufs(dev, name, 0, bufs_count, alloc_hints);
    obj_cache_invalidate_bufs(obj_bufs);

    obj_cache_release_bufs(obj_bufs);
  }

  err = delete_object(dev, name);
  if (NO_ERR != err) {
    return err;
  }

  return NO_ERR;
}

uint objects_cache_hits() { return hits; }

uint objects_cache_misses() { return misses; }
