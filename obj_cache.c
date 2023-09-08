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
#define OBJECT_CACHE_METADATA_BLOCK_NUMBER -1

#endif


struct sleeplock cachelock;

uint hits;
uint misses;


struct obj_cache_metadata_block{
  uint object_size;
};

struct obj_cache_data_block{
  uchar data[CACHE_BLOCK_SIZE];
  uint size;
};

typedef union obj_cache_block_content
{
  struct obj_cache_data_block data_block;
  struct obj_cache_metadata_block metadata_block;
  
}obj_cache_block_content;


typedef struct obj_cache_entry {
  char object_id[OBJECT_ID_LENGTH];
  uint block_no;
  struct obj_cache_entry* prev;
  struct obj_cache_entry* next;
  obj_cache_block_content content;
}obj_cache_entry;

struct {
  obj_cache_entry entries[OBJECTS_CACHE_ENTRIES];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used. The head itself doesn't keep an object.
  obj_cache_entry head;
} obj_cache;



void _cache_invalidate_entry(obj_cache_entry* e){
  e -> object_id[0] = 0;
}

bool _cache_is_entry_valid(obj_cache_entry* e){
  return e -> object_id[0] == 0;
}

void _init_object_entry(obj_cache_entry* e) {
  _cache_invalidate_entry(e);  // empty string
}

void init_objects_cache() {
  hits = 0;
  misses = 0;

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

uint _offset_to_block_no(uint offset){
  /***gets an offset within an object and returns in which block the offset is stored
   */
  return offset / CACHE_BLOCK_SIZE;
}

uint _cache_add_data_entry(const char* id, char* data, uint size, uint block_no){
  obj_cache_entry* e = obj_cache.head.prev;
  _move_to_front(e);
  e->content.data_block.size = size;
  e->block_no = block_no;
  memmove(e->content.data_block.data, data, size);
  memmove(e->object_id, id, obj_id_bytes(id));
  return NO_ERR;
}

uint _cache_add_metadata_entry(const char* id, uint object_size){
  obj_cache_entry* e = obj_cache.head.prev;
  _move_to_front(e);
  e->content.metadata_block.object_size = object_size;
  e->block_no = OBJECT_CACHE_METADATA_BLOCK_NUMBER;
  return NO_ERR;
}

obj_cache_entry* _cache_get_entry(const char* id, uint block_no){
  // get an entry from the cache. the efficiency problems of this function will be solved when I implement a hash map
  for (obj_cache_entry* e = obj_cache.head.prev; e != &obj_cache.head;
       e = e->prev) {
    if (obj_id_cmp(id, e->object_id) == 0 && block_no == e -> block_no) {
      hits ++;
      return e;
    }
  }
  misses ++;
  return NULL;
}

uint _cache_remove_entry(const char* id, uint block_no){
  obj_cache_entry* e = _cache_get_entry(id, block_no);
  if (e != NULL){
    _move_to_back(e);
    _cache_invalidate_entry(e);
    return NO_ERR;
  }
  return OBJECT_NOT_EXISTS;
}

uint cache_remove_object(const char* id, uint offset, bool remove_metadata) {
  acquiresleep(&cachelock);
  if (remove_metadata){
    _cache_remove_entry(id, OBJECT_CACHE_METADATA_BLOCK_NUMBER);
  }

  for (uint block_no = _offset_to_block_no(offset); block_no < CACHE_MAX_BLOCKS_PER_OBJECT; block_no++){
    _cache_remove_entry(id, block_no);
  }
  releasesleep(&cachelock);
  return NO_ERR;
}

uint cache_delete_object(const char* id) {
  acquiresleep(&cachelock);
  
  cache_remove_object(id, 0, true);

  // delete the object from the disk
  uint rv = delete_object(id);
  if (rv != NO_ERR) {
    releasesleep(&cachelock);
    return rv;
  }

  releasesleep(&cachelock);
  return NO_ERR;
}

uint cache_object_size(const char* id, uint* output) {
  acquiresleep(&cachelock);
  obj_cache_entry* e = _cache_get_entry(id, OBJECT_CACHE_METADATA_BLOCK_NUMBER);
  if (e != NULL){
    releasesleep(&cachelock);
    return e->content.metadata_block.object_size;
  }
  else{
    uint err = object_size(id, output);
    _cache_add_metadata_entry(id, *output);
    releasesleep(&cachelock);
    return err;
  }
}

uint cache_get_object(const char* id, vector* outputvector,
                      const uint start_offset, const uint end_offset) {
  acquiresleep(&cachelock);
  uint resolved_end_offset = end_offset;
  if(end_offset == OBJ_END){
    cache_object_size(id, &resolved_end_offset);
  }


  //If the object is too big for the cache, read the object from the disk and return
  uint size;
  if (object_size(id, &size) != NO_ERR) {
    releasesleep(&cachelock);
    panic("cache get object failed to get object size");
  }
  if (size > CACHE_MAX_OBJECT_SIZE) {
    vector temp = newvector(size, 1);
    uint rv = get_object(id, NULL, &temp);
    copysubvector(outputvector, &temp, start_offset, resolved_end_offset - start_offset + 1);
    freevector(&temp);
    releasesleep(&cachelock);
    return rv;
  }

  uint start_offset_block_start = (start_offset / CACHE_BLOCK_SIZE) * CACHE_BLOCK_SIZE;
  uint end_offset_block_start = (resolved_end_offset / CACHE_BLOCK_SIZE) * CACHE_BLOCK_SIZE;
  
  // if we are missing a part of the object, we will read the object from the disk
  bool is_object_read_from_disk = false;
  char data[CACHE_BLOCK_SIZE];
  uint curr_offset = 0;
  vector object_holder = newvector(size, 1);

  //go through the relevant blocks. if a block doesnt exist in the cache, read the object from memory, and if it exists use it to built the result vector
  for (uint block_no = _offset_to_block_no(start_offset); block_no < _offset_to_block_no(resolved_end_offset); block_no ++){
    obj_cache_entry* e = _cache_get_entry(id, block_no);
    if (e == NULL){
      if (is_object_read_from_disk == false){
        is_object_read_from_disk = true;
        get_object(id, NULL, &object_holder);
      }
      memmove_from_vector(data, object_holder, block_no * CACHE_BLOCK_SIZE, CACHE_BLOCK_SIZE);
      _cache_add_data_entry(id, data, CACHE_BLOCK_SIZE, block_no);
    }
    else{
      memmove(&data, e->content.data_block.data, CACHE_BLOCK_SIZE);
    }
    memmove_into_vector_bytes(*outputvector, curr_offset, data, CACHE_BLOCK_SIZE);
    curr_offset =+ CACHE_BLOCK_SIZE;
  }

  // handling the last block
  obj_cache_entry* e = _cache_get_entry(id, _offset_to_block_no(resolved_end_offset));
  if (e == NULL){
    if (is_object_read_from_disk == false){
      object_holder = newvector(size, 1);
      get_object(id, NULL, &object_holder);
    } 
    copysubvector(outputvector, &object_holder, start_offset_block_start, resolved_end_offset - start_offset_block_start); 
    
    uint size_to_cache = CACHE_BLOCK_SIZE;
    if (size - end_offset_block_start < CACHE_BLOCK_SIZE){
      size_to_cache = size - end_offset_block_start;
    }

    memmove_from_vector(data, object_holder, end_offset_block_start, size_to_cache);
    _cache_add_data_entry(id, data, size_to_cache, _offset_to_block_no(resolved_end_offset));    
  }
  if (is_object_read_from_disk){
    freevector(&object_holder);
  }
  *outputvector = slicevector(*outputvector, start_offset - start_offset_block_start, outputvector -> vectorsize - (start_offset - start_offset_block_start));

  releasesleep(&cachelock);
  return NO_ERR;
}

uint objects_cache_hits() { return hits; }

uint objects_cache_misses() { return misses; }

uint cache_max_object_size() { return CACHE_MAX_OBJECT_SIZE; }
