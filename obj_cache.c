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

#define _offset_to_block_no(offset) ((offset) / CACHE_BLOCK_SIZE)
#define _block_no_to_start_offset(block_no) ((block_no) * CACHE_BLOCK_SIZE)
#endif


struct sleeplock cachelock;

uint hits;
uint misses;
vector last_object_from_disk;
char last_object_from_disk_id[MAX_OBJECT_NAME_LENGTH];

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

uint _cache_remove_object_unsafe(const char* id, uint offset, bool remove_metadata) {
  if (remove_metadata){
    _cache_remove_entry(id, OBJECT_CACHE_METADATA_BLOCK_NUMBER);
  }

  for (uint block_no = _offset_to_block_no(offset); block_no < CACHE_MAX_BLOCKS_PER_OBJECT; block_no++){
    _cache_remove_entry(id, block_no);
  }
  return NO_ERR;
}

uint cache_remove_object(const char* id, uint offset, bool remove_metadata) {
  acquiresleep(&cachelock);
  uint rv = _cache_remove_object_unsafe(id, offset, remove_metadata);
  releasesleep(&cachelock);
  return rv;
}

uint cache_delete_object(const char* id) {
  acquiresleep(&cachelock);
  if (strcmp(id, last_object_from_disk_id) == 0){
    last_object_from_disk_id[0] = '\0';
  }
  
  _cache_remove_object_unsafe(id, 0, true);

  // delete the object from the disk
  uint rv = delete_object(id);
  releasesleep(&cachelock);
  return rv;
}

uint _cache_object_size_unsafe(const char* id, uint* output) {
  obj_cache_entry* e = _cache_get_entry(id, OBJECT_CACHE_METADATA_BLOCK_NUMBER);
  if (e != NULL){
    *output = e->content.metadata_block.object_size;
    return NO_ERR;
  }
  else{
    uint err = object_size(id, output);
    _cache_add_metadata_entry(id, *output);
    return err;
  }
}


uint cache_object_size(const char* id, uint* output) {
  acquiresleep(&cachelock);
  uint rv = _cache_object_size_unsafe(id, output);
  releasesleep(&cachelock);
  return rv;
}

uint _cache_read_object_from_disk(const char* id){
  if(strcmp(last_object_from_disk_id, id) == 0){
    return NO_ERR;
  }
  freevector(&last_object_from_disk);
  strncpy(last_object_from_disk_id, id, MAX_OBJECT_NAME_LENGTH);

  uint size;
  if(_cache_object_size_unsafe(id, &size) != NO_ERR){
    panic("cache read object from disk failed to get object size");
  }

  last_object_from_disk = newvector(size, 1);
  return get_object(id, NULL, &last_object_from_disk);
}


uint _cache_get_block(const char* id, uint block_no, char* data_output, uint* size_output){
  obj_cache_entry* e = _cache_get_entry(id, block_no);
  if(e != NULL){
    *size_output = e->content.data_block.size;
    memmove(data_output, e->content.data_block.data, *size_output);
    return NO_ERR;
  }

  uint size;
  uint err = _cache_object_size_unsafe(id, &size);
  if(err != NO_ERR){
    return err;
  }
  uint block_size = min(CACHE_BLOCK_SIZE, size - block_no * CACHE_BLOCK_SIZE); 
  *size_output = block_size;
  
  _cache_read_object_from_disk(id);
  memmove_from_vector(data_output, last_object_from_disk, block_no * CACHE_BLOCK_SIZE, block_size);
  _cache_add_data_entry(id, data_output, block_size, block_no);
  return NO_ERR; 
}



uint cache_get_object(const char* id, vector* outputvector,
                      const uint start_offset, const uint end_offset) {
  //return get_object(id, NULL, outputvector);
  acquiresleep(&cachelock);
  
  uint size;
  if(object_size(id, &size) != NO_ERR) {
    releasesleep(&cachelock);
    panic("cache get object failed to get object size");
  }  

  // Determine end offset
  uint resolved_end_offset = (end_offset == OBJ_END) ?  size - 1 : end_offset;

  // If the object is too big for the cache, read the object from the disk and return
  if (size > CACHE_MAX_OBJECT_SIZE) {
    vector temp = newvector(size, 1);
    uint rv = get_object(id, NULL, &temp);
    copysubvector(outputvector, &temp, start_offset, resolved_end_offset - start_offset + 1);
    freevector(&temp);
    releasesleep(&cachelock);
    return rv;
  }

  for(uint curr_block = _offset_to_block_no(start_offset); curr_block <= _offset_to_block_no(resolved_end_offset); curr_block++){
    uint block_size;
    char block_data[CACHE_BLOCK_SIZE];
    uint err = _cache_get_block(id, curr_block, block_data, &block_size);
    if(err != NO_ERR){
      releasesleep(&cachelock);
      return err;
    }
    memmove_into_vector_bytes(*outputvector, _block_no_to_start_offset(curr_block), block_data, min(block_size, (resolved_end_offset - _block_no_to_start_offset(curr_block) + 1)));
  }
  
  uint start_offset_delta = start_offset - _block_no_to_start_offset(_offset_to_block_no(start_offset));
  *outputvector = slicevector(*outputvector, start_offset_delta, outputvector -> vectorsize);

  releasesleep(&cachelock);
  return NO_ERR;
}

uint objects_cache_hits() { return hits; }

uint objects_cache_misses() { return misses; }

uint cache_max_object_size() { return CACHE_MAX_OBJECT_SIZE; }
