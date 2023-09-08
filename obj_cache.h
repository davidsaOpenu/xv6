#ifndef XV6_OBJ_CACHE_H
#define XV6_OBJ_CACHE_H

/**
 * Object Buffer Cache
 * ~~~~~~~~~~~~~~~~~~~
 *
 * Goal
 * ~~~~
 * The goal of this cache- is the same goal a most buffer caches- which is to
 * save disk operations by saving files in memory- and accessing the memory
 * version of a file instead of accessing the disk- as reading from the memory
 * is orders of magnitude faster than reading from the disk.
 *
 *  General Description
 * ~~~~~~~~~~~~~~~~~~~
 * The object cache is the second layer of the object file system.
 * Our cache is an LRU cache that saves object parts in units of 1kb called
 * blocks.
 *
 * Cache Structure
 * ~~~~~~~~~~~~~~~
 * This cache is a linked list of entries
 * that contain info about objects in disk. Each link is defined in the struct
 * obj_cache_entry.
 *
 * Block Types
 * ~~~~~~~~~~~
 * Cache blocks may saves two different types of information:
 *  - The first is object data- it is saved in a data block in units of 1kb.
 *    Up to 8 blocks of data can be saved of each object- if an object is bigger
 *    than 8kb we dont cache it because the space we have in memory is limited
 * and we dont want to save objets that are too big- if an objet is 100GB and we
 *    read it's blocks one after the other we dont want it to take the whole
 * space of the cache.
 *
 *  - The second type of information that we save is object metadata- we save
 *    it in blocks called metadata blocks. Each metadata block contains the size
 * of an entire object.
 *
 * API
 * ~~~
 * The api works with offsets in objects and with object ids- the cache design
 * is transparent to the user, and is very similar to the direct disk access
 * api.
 *
 * The API described in this file gives an abstraction of disk operation method,
 * and every disk access should be done with those methods- direct access to the
 * disk can cause currupted data in cache- so only use direct disk access if you
 * know what you are doing.
 *
 * Eviction Strategy
 * ~~~~~~~~~~~~~~~~~
 * This is an LRU cache with 800 entries- when the cache is full and we want to
 * add a new object to the cache we take the last object in the cache linked
 * list- which is the entry that was not accessed for the longest time and use
 * its entry
 *
 *
 * Write policy
 * ~~~~~~~~~~~~
 * Currently, the write policy is immediate writes to the disk- the disk and the
 * cache are always in sync.
 *
 * Statistics
 * ~~~~~~~~~~
 * Each time a user uses a cache operation, if the operation made us read from
 * the disk, we will increment the hits counter, and if it didn't we will
 * increment the misses counter. The cache statistics can be used with the
 * functions at the end of this file, and they are accessible in usermode
 * through the procfs
 *
 * Testing
 * ~~~~~~~
 * The functionality of this cache is tested in tests/xv6/objfstests.c
 *
 */

#include "kvector.h"
#include "types.h"

// if this value is passed to get_object as the end offset the object will be
// read to the end
#define OBJ_END -1

/**
 * Initialize the cache by making all entries invalid, and marking the
 * last_object_from_disk as invalid.
 */
void init_objects_cache();

/**
 * remove an object from the cache and delete it from the disk
 *
 * @param  id: the object's id.
 * @return the error code that delete_object returns
 */
uint cache_delete_object(const char* id);

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
                      const uint start_offset, const uint end_offset);

/**
 * remove an object from the cache from a certain offset while locking the cache
 *
 * @param  id: the object's id.
 * @param  offset: the offset in bytes from which the objcet data will be
 * deleted from the cache.
 * @return  error code (currently always returns NO_ERR).
 */
uint cache_prune_object(const char* id, uint offset);

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
                          uint write_starting_offset, const char* id);

/**
 * The following methods provides statistics about the cache layer. They can
 * used by program to show performance of the file system or to try and
 * optimize their run flow.
 * they can be access from /proc from usermode
 */

uint objects_cache_hits();
uint objects_cache_misses();
uint cache_max_object_size();
uint cache_block_size();
uint cache_blocks();
uint cache_blocks_per_object();
#endif /* XV6_OBJ_CACHE_H */
