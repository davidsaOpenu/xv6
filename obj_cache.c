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
#define OBJECTS_CACHE_ENTRIES 800
#define OBJECT_CACHE_METADATA_BLOCK_NUMBER CACHE_MAX_BLOCKS_PER_OBJECT

#define _offset_to_block_no(offset) ((offset) / CACHE_BLOCK_SIZE)
#define _block_no_to_start_offset(block_no) ((block_no)*CACHE_BLOCK_SIZE)
#endif

struct sleeplock cachelock;

uint hits;
uint misses;
vector last_object_from_disk;
char last_object_from_disk_id[MAX_OBJECT_NAME_LENGTH + 1];

/**
 * This struct is an entry that contains data that is not part of the object
 * data, currently it only contains size but may contain other data in the
 * future
 */
struct obj_cache_metadata_block {
  uint object_size;
};

/**
 * this is the struct that contains the actual data of the object
 */
struct obj_cache_data_block {
  char data[CACHE_BLOCK_SIZE];
  uint size;
};

/**
 * this is a union that represents the entry content
 */
typedef union obj_cache_block_content {
  struct obj_cache_data_block data_block;
  struct obj_cache_metadata_block metadata_block;
} obj_cache_block_content;

/**
 * this struct represents a cache node
 */
typedef struct obj_cache_entry {
  char object_id[OBJECT_ID_LENGTH];
  // the number of the block out of CACHE_MAX_BLOCKS_PER_OBJECT
  uint block_no;
  struct obj_cache_entry* prev;
  struct obj_cache_entry* next;
  obj_cache_block_content content;
} obj_cache_entry;

struct {
  // an array that contains all the cache entries
  obj_cache_entry entries[OBJECTS_CACHE_ENTRIES];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used. The head itself doesn't keep an object.
  obj_cache_entry head;
} obj_cache;

/**
 * make a cache entry invalid by giving it an empty id
 */
void _cache_invalidate_entry(obj_cache_entry* e) { e->object_id[0] = '\0'; }

/**
 * if the id of the entry is empty the entry is not valid
 */
bool _cache_is_entry_valid(obj_cache_entry* e) {
  return e->object_id[0] != '\0';
}

void _init_object_entry(obj_cache_entry* e) { _cache_invalidate_entry(e); }

void init_objects_cache() {
  hits = 0;
  misses = 0;
  // we save the object last taken from the disk so that we won't have to access
  // the disk to get each cache block
  last_object_from_disk_id[0] = '\0';
  last_object_from_disk = newvector(1, 1);
  initsleeplock(&cachelock, "cachelock");

  // the following, was copied from `bio.c` with minor changes.
  obj_cache_entry* e;
  obj_cache.head.prev = &obj_cache.head;
  obj_cache.head.next = &obj_cache.head;
  for (e = obj_cache.entries; e < obj_cache.entries + OBJECTS_CACHE_ENTRIES;
       e++) {
    _init_object_entry(e);
    e->next = obj_cache.head.next;
    e->prev = &obj_cache.head;
    obj_cache.head.next->prev = e;
    obj_cache.head.next = e;
  }
}

static void _move_to_front(obj_cache_entry* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
  e->next = obj_cache.head.next;
  e->prev = &obj_cache.head;
  obj_cache.head.next->prev = e;
  obj_cache.head.next = e;
}

static void _move_to_back(obj_cache_entry* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
  e->next = &obj_cache.head;
  e->prev = obj_cache.head.prev;
  obj_cache.head.prev->next = e;
  obj_cache.head.prev = e;
}

/**
 * add a data block to the cache
 * :param id: object id
 * :param data: the block data
 * :param size: the size of the data in the block (not the whole object)
 * :param block_no: the number of the block
 */
uint _cache_add_data_entry(const char* id, char* data, uint size,
                           uint block_no) {
  obj_cache_entry* e = obj_cache.head.prev;
  _move_to_front(e);
  e->content.data_block.size = size;
  e->block_no = block_no;
  memmove(e->content.data_block.data, data, size);
  memmove(e->object_id, id, obj_id_bytes(id));
  return NO_ERR;
}

/**
 * add a metadata block to the cache
 * :param id: object id
 * :param objet_size: the size of the entire object
 */
uint _cache_add_metadata_entry(const char* id, uint object_size) {
  obj_cache_entry* e = obj_cache.head.prev;
  _move_to_front(e);
  e->content.metadata_block.object_size = object_size;
  e->block_no = OBJECT_CACHE_METADATA_BLOCK_NUMBER;
  return NO_ERR;
}

/**
 * get a pointer to a requested entry
 * :param id: object's id
 * :param block_no: the index of the requested block
 */
obj_cache_entry* _cache_get_entry(const char* id, uint block_no) {
  // get an entry from the cache.
  for (obj_cache_entry* e = obj_cache.head.next; e != &obj_cache.head;
       e = e->next) {
    if (!obj_id_cmp(id, e->object_id) && block_no == e->block_no) {
      return e;
    }
  }
  return NULL;
}

/**
 * remove an entry from the cache
 * :param id: objet's id
 * :param block_no: the index of the block that will be deleted from the cache
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
 * without taking the cache's lock :param id: the object's id :param offset: the
 * offset in bytes from which the objcet data will be deleted from the cache
 */
uint _cache_remove_object_unsafe(const char* id, uint offset) {
  _cache_remove_entry(id, OBJECT_CACHE_METADATA_BLOCK_NUMBER);
  for (uint block_no = _offset_to_block_no(offset);
       block_no < CACHE_MAX_BLOCKS_PER_OBJECT; block_no++) {
    _cache_remove_entry(id, block_no);
  }
  return NO_ERR;
}

/**
 * remove an object from the cache from a certain offset while locking the cache
 * :param id: the object's id
 * :param offset: the offset in bytes from which the objcet data will be deleted
 * from the cache
 */
uint cache_remove_object(const char* id, uint offset) {
  acquiresleep(&cachelock);
  uint rv = _cache_remove_object_unsafe(id, offset);
  releasesleep(&cachelock);
  return rv;
}

/**
 * remove an object from the cache and delete it from the disk
 * :param id: the object's id
 */
uint cache_delete_object(const char* id) {
  acquiresleep(&cachelock);

  _cache_remove_object_unsafe(id, 0);

  // delete the object from the disk
  uint rv = delete_object(id);
  releasesleep(&cachelock);
  return rv;
}

/**
 * remove an object from the cache and then call rewrite_object
 * :param data: new object's data
 * :param objectsize: new object size
 * :param write_starting_offset: offset to rewrite from
 * :param id: the object's id
 */
uint cache_rewrite_object(vector data, uint objectsize,
                          uint write_starting_offset, const char* id) {
  acquiresleep(&cachelock);
  if (_cache_remove_object_unsafe(id, 0) != NO_ERR) {
    panic("cache rewrite object could not remove an object from the cache");
  }
  releasesleep(&cachelock);
  return rewrite_object(data, objectsize, write_starting_offset, id);
}

/**
 * get the size of the object from the cache if possible and from the disk if
 * not, while not taking the cache lock :param id: the object's id :param
 * output: will be set to the object's size :param update_statistics: if true
 * update hits and misses
 */
uint _cache_object_size_unsafe(const char* id, uint* output,
                               bool update_statistics) {
  obj_cache_entry* e = _cache_get_entry(id, OBJECT_CACHE_METADATA_BLOCK_NUMBER);
  if (e != NULL) {
    *output = e->content.metadata_block.object_size;
    if (update_statistics) {
      hits++;
    }
  } else {
    uint err = object_size(id, output);
    if (err != NO_ERR) {
      return err;
    }
    if (update_statistics) {
      misses++;
    }
  }
  return NO_ERR;
}

/**
 * get an objects size while locking the cache
 * :param id: the object's id
 * :param output: will be set to the object's size
 */
uint cache_object_size(const char* id, uint* output) {
  acquiresleep(&cachelock);
  uint rv = _cache_object_size_unsafe(id, output, true);
  releasesleep(&cachelock);
  return rv;
}

/**
 * update last_object_from_disk to the requested object if it's not that object
 * already :param id: id of the object to read from disk
 */
uint _cache_read_object_from_disk(const char* id) {
  if (obj_id_cmp(last_object_from_disk_id, id) == 0) {
    return NO_ERR;
  }
  freevector(&last_object_from_disk);
  strncpy(last_object_from_disk_id, id, MAX_OBJECT_NAME_LENGTH);
  last_object_from_disk_id[MAX_OBJECT_NAME_LENGTH] = '\0';

  uint size;
  if (_cache_object_size_unsafe(id, &size, false) != NO_ERR) {
    panic("cache read object from disk failed to get object size");
  }

  last_object_from_disk = newvector(size, 1);
  return get_object(id, NULL, &last_object_from_disk);
}

/**
 * get a cache block from the cache if possible, and from the disk if not
 * :param id: the object's id
 * :param block_no: the requested block
 * :param data_output: a buffer that will be set to the data of the block, has
 * to be at least as big as the block's size :param size_output: will be set to
 * the size of the block
 */
uint _cache_get_block(const char* id, uint block_no, char* data_output,
                      uint* size_output) {
  obj_cache_entry* e = _cache_get_entry(id, block_no);
  if (e != NULL) {
    *size_output = e->content.data_block.size;
    memmove(data_output, (char*)(e->content.data_block.data),
            e->content.data_block.size);
    return NO_ERR;
  }

  uint size;
  uint err = _cache_object_size_unsafe(id, &size, false);
  if (err != NO_ERR) {
    panic("cache get block could not get size");
  }
  uint distance_from_end = size - block_no * CACHE_BLOCK_SIZE;
  uint block_size = CACHE_BLOCK_SIZE < distance_from_end ? CACHE_BLOCK_SIZE
                                                         : distance_from_end;
  *size_output = block_size;
  if (_cache_read_object_from_disk(id) != NO_ERR) {
    panic("could not read object from disk in cache get block");
  }
  memmove_from_vector(data_output, last_object_from_disk,
                      block_no * CACHE_BLOCK_SIZE, block_size);
  _cache_add_data_entry(id, data_output, block_size, block_no);
  return NO_ERR;
}

/**
 * get an object from the cache if possible, and from the disk if not.
 * :param id: the object's id
 * :param outputvector: will be set to the data of the block
 * :param start_offset: the offset to get the object from
 * :param end_offset: the offset to read up to
 */
uint cache_get_object(const char* id, vector* outputvector,
                      const uint start_offset, const uint end_offset) {
  acquiresleep(&cachelock);
  last_object_from_disk_id[0] = '\0';
  uint size;
  uint err = _cache_object_size_unsafe(id, &size, false);
  if (err != NO_ERR) {
    releasesleep(&cachelock);
    panic("cache get object failed to get object size");
  }

  // Determine end offset
  uint resolved_end_offset = (end_offset == OBJ_END) ? size - 1 : end_offset;
  vector temp_vector = newvector(size + CACHE_BLOCK_SIZE, 1);
  // If the object is too big for the cache, read the object from the disk and
  // return
  if (size > CACHE_MAX_OBJECT_SIZE || resolved_end_offset < 0) {
    uint rv = get_object(id, NULL, &temp_vector);
    copysubvector(outputvector, &temp_vector, start_offset,
                  resolved_end_offset - start_offset + 1);
    freevector(&temp_vector);
    releasesleep(&cachelock);
    return rv;
  }

  for (uint curr_block = _offset_to_block_no(start_offset);
       curr_block <= _offset_to_block_no(resolved_end_offset); curr_block++) {
    uint block_size;
    char block_data[CACHE_BLOCK_SIZE];
    uint err = _cache_get_block(id, curr_block, block_data, &block_size);

    if (err != NO_ERR) {
      freevector(&temp_vector);
      releasesleep(&cachelock);
      panic("cache get object could not get block");
      return err;
    }

    uint distance_from_end =
        resolved_end_offset - _block_no_to_start_offset(curr_block) + 1;
    // this could be done with the min function, but cpplint had a problem with
    // that
    uint num_of_bytes_to_copy =
        block_size < distance_from_end ? block_size : distance_from_end;
    memmove_into_vector_bytes(
        temp_vector,
        _block_no_to_start_offset(curr_block -
                                  _offset_to_block_no(start_offset)),
        block_data, num_of_bytes_to_copy);
  }

  copysubvector(outputvector, &temp_vector, start_offset % CACHE_BLOCK_SIZE,
                resolved_end_offset - start_offset + 1);
  freevector(&temp_vector);
  releasesleep(&cachelock);
  if (last_object_from_disk_id[0] == '\0') {
    hits++;
  } else {
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
