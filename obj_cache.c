#include "obj_cache.h"

#include "obj_disk.h"
#include "sleeplock.h"
#include "string.h"

#ifndef KERNEL_TESTS
#include "defs.h"  // import `panic`
#else
#include "obj_fs_tests_utilities.h"  // impot mock `panic`
#endif

#ifndef OBJ_CACHE_CONSTS
#define OBJ_CACHE_CONSTS

#define CACHE_BLOCK_SIZE 1024
#define CACHE_MAX_BLOCKS_PER_OBJECT 8
#define CACHE_MAX_OBJECT_SIZE (CACHE_BLOCK_SIZE * CACHE_MAX_BLOCKS_PER_OBJECT)
#define OBJECTS_CACHE_ENTRIES 100

#define _offset_to_block_no(offset) ((offset) / CACHE_BLOCK_SIZE)
#define _block_no_to_start_offset(block_no) ((block_no)*CACHE_BLOCK_SIZE)
#endif

struct sleeplock cachelock;

// Global variables that are used to save cache statistics. they are global
// because they save general cache state
uint hits;
uint misses;

// Global variables that are used to save the current object that the cache
// works with. They are global because they keeps state that many functions use
// and we would have to pass them to many functions if they were not global. Use
// can be seen at _cache_read_objet_from_disk
vector last_object_from_disk;
char last_object_from_disk_id[MAX_OBJECT_NAME_LENGTH + 1];

/**
 * this struct represents a cache node
 * object_id- the name of the object
 * block_no-
 */
typedef struct obj_cache_entry {
  // the name of the object- the same id that the object has in the disk
  char object_id[OBJECT_ID_LENGTH];
  // This is the offset that is saved in the node in blocks
  uint block_no;
  // There fields exist to maintain the cache as a linked list
  struct obj_cache_entry* prev;
  struct obj_cache_entry* next;
  // The data of the block.
  char data[CACHE_BLOCK_SIZE];
  uint block_size;
} obj_cache_entry;

/**
 * This is the struct that contains the entire cache.
 */
struct {
  // An array that contains all the cache entries- the array will be used
  // as an array only for initialization and then the entries will be used
  // as a linked list
  obj_cache_entry entries[OBJECTS_CACHE_ENTRIES];

  // The entries linked list is a circular list. Head is an entry that we
  // use as a fixed point in the linked listHead.next is the newest entry
  // in the cache and head.prev is the oldest.
  obj_cache_entry head;
} obj_cache;

/**
 * make a cache entry invalid by giving it an empty id
 */
void _cache_invalidate_entry(obj_cache_entry* e) { e->object_id[0] = '\0'; }

/**
 * if the id of the entry is empty the entry is not valid.
 * return true if an entry contains valid data, false otherwise
 */
bool _cache_is_entry_valid(obj_cache_entry* e) {
  return e->object_id[0] != '\0';
}

/**
 * initialize an entry by marking the initial data there as invalid
 */
void _init_object_entry(obj_cache_entry* e) { _cache_invalidate_entry(e); }

/**
 * Initialize the cache by making all entries invalid, and marking the
 * last_object_from_disk as invalid.
 */
void init_objects_cache() {
  hits = 0;
  misses = 0;
  // Make the last object from disk invalid so we wouldn't get corrupted data
  // there
  last_object_from_disk_id[0] = '\0';
  last_object_from_disk = newvector(1, 1);
  initsleeplock(&cachelock, "cachelock");

  // The following, was copied from `bio.c` with minor changes.
  obj_cache_entry* e;
  // Create a linked list that only contains head
  obj_cache.head.prev = &obj_cache.head;
  obj_cache.head.next = &obj_cache.head;

  // Go through each node and add it to the list
  for (e = obj_cache.entries; e < obj_cache.entries + OBJECTS_CACHE_ENTRIES;
       e++) {
    _init_object_entry(e);

    // Insert the entry at the head of the list
    e->next = obj_cache.head.next;
    e->prev = &obj_cache.head;
    obj_cache.head.next->prev = e;
    obj_cache.head.next = e;
  }
}

/**
 * Move a cache entry to the head of the cache linked list.
 * Used when a node is accessed- so we want the LRU to evict it last
 *
 * @param  e: The entry to move to the front
 */
static void _move_to_front(obj_cache_entry* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
  e->next = obj_cache.head.next;
  e->prev = &obj_cache.head;
  obj_cache.head.next->prev = e;
  obj_cache.head.next = e;
}

/**
 * Move a cache entry to the back of the cache linked list.
 * Used when a node is deleted from the cache- so that it would
 * be taken when there is a write to the cache.
 *
 * @param  e: The entry to move to the back.
 */
static void _move_to_back(obj_cache_entry* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
  e->next = &obj_cache.head;
  e->prev = obj_cache.head.prev;
  obj_cache.head.prev->next = e;
  obj_cache.head.prev = e;
}

/**
 * add a block to the cache.
 *
 * @param  id: object id.
 * @param  data: the block data.
 * @param  size: the size of the data in the block (not the whole object).
 * @param  block_no: the number of the block.
 * @return   error code (currently only returns NO_ERR).
 */
uint _cache_add_entry(const char* id, char* data, uint size, uint block_no) {
  obj_cache_entry* e = obj_cache.head.prev;
  _move_to_front(e);
  e->block_size = size;
  e->block_no = block_no;
  memmove(e->data, data, size);
  memmove(e->object_id, id, obj_id_bytes(id));
  return NO_ERR;
}

/**
 * get a pointer to an entry specified by id and block number
 *
 * @param  id: object's id.
 * @param  block_no: the index of the requested block.
 * @return  a pointer to the requested entry.
 */
obj_cache_entry* _cache_get_entry(const char* id, uint block_no) {
  // Go through the cache
  for (obj_cache_entry* e = obj_cache.head.next; e != &obj_cache.head;
       e = e->next) {
    // if the entry is the requested entry
    if (!obj_id_cmp(id, e->object_id) && block_no == e->block_no) {
      return e;
    }
  }
  return NULL;
}

/**
 * remove an entry from the cache
 *
 * @param  id: objet's id.
 * @param  block_no: the index of the block that will be deleted from the cache.
 * @return  error code: returns NO_ERR if everything is fine and
 * OBJECT_NOT_EXISTS if the entry could not be found.
 */
uint _cache_remove_entry(const char* id, uint block_no) {
  obj_cache_entry* e = _cache_get_entry(id, block_no);
  if (e == NULL) {
    return OBJECT_NOT_EXISTS;
  }

  _move_to_back(e);
  _cache_invalidate_entry(e);
  return NO_ERR;
}

/**
 * remove all the entries of an object from a certain offset from the cache
 * without taking the cache's lock
 *
 * @param  id: the object's id.
 * @param  offset: the offset in bytes from which the objcet data will be
 * deleted from the cache.
 * @return  error code (currently always returns NO_ERR).
 */
uint _cache_prune_object_unsafe(const char* id, uint offset) {
  for (uint block_no = _offset_to_block_no(offset);
       block_no < CACHE_MAX_BLOCKS_PER_OBJECT; block_no++) {
    _cache_remove_entry(id, block_no);
  }
  return NO_ERR;
}

/**
 * remove an object from the cache from a certain offset while locking the cache
 *
 * @param  id: the object's id.
 * @param  offset: the offset in bytes from which the objcet data will be
 * deleted from the cache.
 * @return  error code (currently always returns NO_ERR).
 */
uint cache_prune_object(const char* id, uint offset) {
  acquiresleep(&cachelock);
  uint rv = _cache_prune_object_unsafe(id, offset);
  releasesleep(&cachelock);
  return rv;
}

/**
 * remove an object from the cache and delete it from the disk
 *
 * @param  id: the object's id.
 * @return the error code that delete_object returns
 */
uint cache_delete_object(const char* id) {
  acquiresleep(&cachelock);

  _cache_prune_object_unsafe(id, 0);

  // delete the object from the disk
  uint rv = delete_object(id);
  releasesleep(&cachelock);
  return rv;
}

/**
 * remove an object from the cache and then call rewrite_object
 *
 * @param  data: new object's data.
 * @param  objectsize: new object size.
 * @param  write_starting_offset: offset to rewrite from.
 * @param  id: the object's id.
 * @return  rewrite_object's return value.
 */
uint cache_rewrite_object(vector data, uint objectsize,
                          uint write_starting_offset, const char* id) {
  acquiresleep(&cachelock);
  if (_cache_prune_object_unsafe(id, 0) != NO_ERR) {
    panic("cache rewrite object could not remove an object from the cache");
  }
  releasesleep(&cachelock);
  return rewrite_object(data, objectsize, write_starting_offset, id);
}

/**
 * get the size of the object from the cache if possible and from the disk if
 * not, while not taking the cache lock.
 *
 * @param  id: the object's id.
 * @param  output: will be set to the object's size.
 * @param  update_statistics: if true update hits and misses
 * @return  object_size's error code if there was an error reading
 * the size from the disk, NO_ERR otherwise.
 */

/**
 * read an object from the disk into the global last_object_from_disk if this
 * object is not the same as <id>. This is done to ensure that there is one
 * object read from the disk per cache operation- subsequent reads will not need
 * to access the disk because the object will already be in
 * last_object_from_disk
 *
 * @param  id: id of the object to read from disk
 * @return the error code of get_object
 */
uint _cache_read_object_from_disk(const char* id) {
  // Check if the object in last_object_from_disk has the right id
  if (obj_id_cmp(last_object_from_disk_id, id) == 0) {
    // if it has the correct id we don't have to do anything and we can return
    return NO_ERR;
  }

  // Free the vector that used to store the previous object
  freevector(&last_object_from_disk);

  // update last_object_from_disk_id
  strncpy(last_object_from_disk_id, id, MAX_OBJECT_NAME_LENGTH);
  last_object_from_disk_id[MAX_OBJECT_NAME_LENGTH] = '\0';

  uint size;
  if (object_size(id, &size) != NO_ERR) {
    panic("cache read object from disk failed to get object size");
  }

  // Put the new object from the disk in a vector and return
  last_object_from_disk = newvector(size, 1);
  return get_object(id, NULL, &last_object_from_disk);
}

/**
 * get a cache block from the cache if possible, and from the disk if not
 *
 * @param  id: the object's id
 * @param  block_no: the requested block
 * @param  data_output: a buffer that will be set to the data of the block, has
 * to be at least as big as the block's size
 * @param  size_output: will be set to the size of the block
 * @return error code- currently always return NO_ERR
 */
uint _cache_get_block(const char* id, uint block_no, char* data_output,
                      uint* size_output) {
  // Look for the requested block in the cache
  obj_cache_entry* e = _cache_get_entry(id, block_no);

  // If the block is in the cache copy it's data and size to the output
  // variables and reuturn
  if (e != NULL) {
    *size_output = e->block_size;
    memmove(data_output, (char*)(e->data), e->block_size);
    return NO_ERR;
  }

  // If the block is not in cache
  uint size;
  uint err = object_size(id, &size);
  if (err != NO_ERR) {
    panic("cache get block could not get size");
  }

  // Calculate the distange of the start of the requested block from the end of
  // the object
  uint distance_from_end = size - block_no * CACHE_BLOCK_SIZE;

  uint block_size;
  if (CACHE_BLOCK_SIZE < distance_from_end) {
    // if there is more than a whole block until the end of the object the
    // block's this block is a whole block
    block_size = CACHE_BLOCK_SIZE;
  } else {
    // if there is less than a whole block until the end of the object the
    // block's this block's size is the bytes that remain until the end of the
    // block
    block_size = distance_from_end;
  }

  *size_output = block_size;

  // We have to read the object from the disk since it's not in the cache
  if (_cache_read_object_from_disk(id) != NO_ERR) {
    panic("could not read object from disk in cache get block");
  }

  // copy the block's data to the data output
  memmove_from_vector(data_output, last_object_from_disk,
                      block_no * CACHE_BLOCK_SIZE, block_size);

  // Cache the block so that we will be able to get it from the cache the next
  // time we need it
  _cache_add_entry(id, data_output, block_size, block_no);
  return NO_ERR;
}

/**
 * get an object from the cache if possible, and from the disk if not.
 *
 * @param  id: the object's id
 * @param  outputvector: will be set to the data of the block
 * @param  start_offset: the offset to get the object from
 * @param  end_offset: the offset to read up to
 * @return the error code of get_object
 */
uint cache_get_object(const char* id, vector* outputvector,
                      const uint start_offset, const uint end_offset) {
  acquiresleep(&cachelock);

  // invalidate last_object_from_disk so that we won't have stale data there
  last_object_from_disk_id[0] = '\0';
  uint size;

  // get the object's size
  uint err = object_size(id, &size);
  if (err != NO_ERR) {
    releasesleep(&cachelock);
    panic("cache get object failed to get object size");
  }

  // Determine end offset
  uint resolved_end_offset;
  // If the end offset is the
  if (end_offset == OBJ_END) {
    resolved_end_offset = size - 1;
  } else {
    resolved_end_offset = end_offset;
  }
  vector temp_vector = newvector(size + CACHE_BLOCK_SIZE, 1);
  // If the object is too big for the cache or if the file is empty, read the
  // object from the disk and return
  if (size > CACHE_MAX_OBJECT_SIZE || resolved_end_offset < 0) {
    uint rv = get_object(id, NULL, &temp_vector);
    copysubvector(outputvector, &temp_vector, start_offset,
                  resolved_end_offset - start_offset + 1);
    freevector(&temp_vector);
    releasesleep(&cachelock);
    return rv;
  }

  // If the object is cacheable

  // Iterate over the blocks of the requested object and fill them from the
  // cache if they are cached, and from the disk otherwise
  for (uint curr_block = _offset_to_block_no(start_offset);
       curr_block <= _offset_to_block_no(resolved_end_offset); curr_block++) {
    uint block_size;
    char block_data[CACHE_BLOCK_SIZE];

    // Get the block
    uint err = _cache_get_block(id, curr_block, block_data, &block_size);

    if (err != NO_ERR) {
      freevector(&temp_vector);
      releasesleep(&cachelock);
      panic("cache get object could not get block");
      return err;
    }

    // The numner of bytes remaining to copy until the end offset
    uint distance_from_end =
        resolved_end_offset - _block_no_to_start_offset(curr_block) + 1;

    // If the end is closer that a single block copy the remaining bytes,
    // otherwise copy a block
    uint num_of_bytes_to_copy =
        block_size < distance_from_end ? block_size : distance_from_end;
    memmove_into_vector_bytes(
        temp_vector,
        _block_no_to_start_offset(curr_block -
                                  _offset_to_block_no(start_offset)),
        block_data, num_of_bytes_to_copy);
  }

  // get rid of the start of the first block by copying a sub vector to the
  // output vector- relevant if we copy the first block from the middle
  copysubvector(outputvector, &temp_vector, start_offset % CACHE_BLOCK_SIZE,
                resolved_end_offset - start_offset + 1);
  freevector(&temp_vector);
  releasesleep(&cachelock);

  // if we didn't read an object from the disk
  if (last_object_from_disk_id[0] == '\0') {
    // We didn't read an object from the disk so this is considered a cache hit
    hits++;
  } else {
    // We accessed the disk so this is considered a cache miss
    misses++;
  }
  return NO_ERR;
}

uint objects_cache_hits() { return hits; }

uint objects_cache_misses() { return misses; }

uint cache_max_object_size() { return CACHE_MAX_OBJECT_SIZE; }

uint cache_block_size() { return CACHE_BLOCK_SIZE; }

uint cache_blocks() { return OBJECTS_CACHE_ENTRIES; }

uint cache_blocks_per_object() { return CACHE_MAX_BLOCKS_PER_OBJECT; }
