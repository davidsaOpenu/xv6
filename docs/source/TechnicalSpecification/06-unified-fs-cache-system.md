# Unified FS Cache System


## Solution Details

The proposed solution maintains the existing layer structure of both nativefs and objfs but introduces a new buffer cache module that serves both file systems, providing a file-system-independent API. Each file system is responsible for adapting this buffer cache module to its specific requirements.

The buffers allocated by the new buffer cache module adhere to the same states as those in the nativefs buffer cache, as detailed in the introduction chapter.

The nativefs utilizes the buffer cache module directly, requesting buffers by device number and block number. This replaces the previous implementation in the bio.c module, which combined buffer caching with other logic. The nativefs now interacts with the new buffer cache module for all buffer-related operations.

In contrast, objfs requires significant adaptation to use the buffer cache module due to its use of variable-sized objects rather than fixed-size blocks. To accommodate this, objfs splits each object into multiple buffers and later reassembles them. This results in multiple buffers being associated with a single object. To uniquely identify each buffer, objfs uses the object ID (name) and the offset within the object’s data.

During read and write operations, the objfs cache layer determines the required buffers and retrieves them from the buffer cache module. If some buffers are invalid, they are validated by reading the entire object from disk. The process involves allocating all buffers for the object and then performing a complete read from the disk.

![](../images/buffers_of_single_object.svg)

*Figure 1: Multiple buffers of a single object in the system cache.*

The API for the objfs logging layer remains largely unchanged, continuing to receive entire objects as parameters, unlike the nativefs cache layer, which operates at the buffer level. Conversely, the objfs disk API now accepts vectors of buffers instead of vectors of content, reflecting its shift to buffer-based operations.

To prevent deadlocks when locking buffers for the same object, it is assumed that no more than one function from the cache layer is invoked concurrently on the same object. This assumption is reasonable since each function operates on the entire object, whose inode should be locked at that time.

As mentioned earlier, we don’t want to cache all the buffers of a big object as it will cause in the future eviction of more useful buffers. Instead, we cache only the buffers overlapping with the region being read or written to, plus some adjacent buffers for padding. This approach leverages locality and sequentiality of reference.  
To implement this, we need to differentiate between these two types of allocated buffer, so we can place them right upon free: if the buffer should be cached, it should be placed first in the MRU list; if the buffer should not be cached, we place it last in the MRU list. This ensures that less useful buffers are evicted first, preserving more valuable buffers for future use.

![](../images/adjacent_buffers.svg)

*Figure 2: Caching only adjacent buffers.*


## API

In this section we discuss the API of the new and modified modules. It is used as a reference for the development, and makes it easier to see the responsibilities and scope of each module. Internal functions will not be discussed here.

### Buffer Cache Module

Main Objective: Allocate and cache fixed-size buffers, according to some caching algorithm.

#### Data Structures

* struct buf  
  Represent a fixed-size buffer. Besides the buffer, it also contains flags used to manage the buffer.  


* union buf\_id  
  A buffer id, used as the key to search the buffer in the cache.  
  Each fs type has its own member in this union, and uses it when getting a buffer.

#### External Functions

##### void buf\_cache\_init(void)  
  Initialize the context of the module

##### struct buf \*buf\_cache\_get(uint dev, const union buf\_id \*id, uint alloc\_flags)  
  Allocate a buffer from the cache, searches first for an existing buffer with the given id and only if not found allocates a new one.  
  Responsible that there will always be at most one buffer representing each id.  
  The ‘alloc\_flags’ param gives a hint about the future usage of this buffer, letting the caching system make better performance decisions.  
 
##### void buf\_cache\_release(struct buf \*b)  
  Release a previously allocated buffer. If there are no more references to this buffer and it is not in a dirty state, it might be recycled and to hold another block.  


##### void buf\_cache\_invalidate\_blocks(uint dev)  
  Invalidate all buffers of a specific device. Can be used when the device is removed.

### Object Cache Module

Main Objective: Manage the cache and buffers of objects. Should be used as read/write through the object storage device.

#### External Functions

##### *uint obj\_cache\_add(uint dev, const char* name, void\* data, uint size)
  Add a new object named ‘name’ to the object storage device, denoted by ‘dev’. Its content might be empty or might be set to the given ‘data’, which is a buffer in the size of ‘size’.

##### *uint obj\_cache\_write(uint dev, const char\* name, void\* data, uint size, uint offset)*
  Modify the content of an already created object named ‘name’, in the offset ‘offset’ according to the content of the given buffer ‘data’, which is a buffer in the size of ‘size’. This operation does not override the existing content of the object in regions not overlapping with the ‘offset’ and ‘size’ params.  
The operation might extend the object size, in this case the new size will be ‘offset’ \+ ‘size’.

##### *uint obj\_cache\_read(uint dev, const char\* name, vector\* dst, uint size, uint offset)*
  Read the content of an already created object named ‘name’, total of ‘size’ bytes from the offset ‘offset’ and write it to the given vector buffer dst.  
The requested region should not exceed the current object size.

##### *uint obj\_cache\_delete(uint dev, const char\* name)*  
  Delete the object named ‘name’ from the device ‘dev’ and also from the cache.


### Object Disk Module

We describe only the modified API of this module, as most of it remains unchanged. The modifications support buffers vector instead of content vector.

Main Objective: Manage and emulate an object storage device.

#### External Functions

##### *uint add\_object(const char\* name, vector bufs, uint size)*
  Add a new object named ‘name’ to the object storage device. Its content might be empty or might be set to the given ‘bufs’, which is a vector of ‘struct buf’, containing total data in size of ‘size’ bytes.

##### *uint write\_object(const char\* name, vector bufs, uint objectsize)*
  Set the content of an already created object named ‘name’, according to the given ‘bufs’, which is a vector of ‘struct buf’, containing total data in size of ‘size’ bytes.  
This operation overrides the existing content of the object, setting its whole content according to the given buffers. The object size might shrink or grow according to the given ‘size’.

##### *uint get\_object(const char\* name, vector bufs)*
  Read the content of an already created object named ‘name’ to the given buffers vector ‘bufs’. The total size pointed by the ‘bufs’ vector should be at least in the object size; otherwise an error will be returned.


## Pseudo-Code

In this section we provide pseudo code of the main functions of the project, which implement the main algorithms and concepts.

### Buffer Cache Module

```
buf_cache_get(dev, id, alloc_flags):
1. Go over the MRU list, in forward direction, and search for the  given 'id'. If found, return it.
2. Otherwise, Go over the MRU list, in backward direction, and search for unreferenced and non-dirty buffer. If found, return it.
3. If no free buffer was found, do panic.
```

```
buf_cache_release(buffer):
1. Decrement the refcount
2. If the refcount reached 0, do:
  2.1 If it was allocated with the "no cached buffer" flag, then:
    2.1.1 Move it to the tail of the MRU list
  2.2 Otherwise, move it the head of the MRU list
```

### Object Cache Module

```
obj_cache_add(dev, name, data):
1. Allocate buffers according to the size of the data, with only the first few blocks as "cached" buffers, and the rest in "not-cached" mode. 
2. Copy the data to the allocated buffers.
3. Add the object to disk, using the obj_disk API.
4. Free the allocated buffers.
```

```
obj_cache_write(dev, name, data, offset):
1. Allocate buffers according to the new size of object.
2. Validate the allocated buffers (either from cache or disk).
3. Copy the given data to allocated buffers, at the given offset.
4. Write the whole object to disk, using the obj_disk API.
4. Free the allocated buffer.
```

```
obj_cache_read(dev, name, dst, size, offset):
1. Allocate the buffers corresponds to the region being read
2. Check if the allocated buffers are valid, if so:
  2.1 Copy the buffers content according to given offset and size to the given dst buffer
  2.2 Free the allocated buffers
3. Otherwise:
  3.1 Free the allocated buffers
  3.2 Allocate all the buffers of the object being read, with only the requested region and its padding as "cached" buffers, and the rest in "not-cached" mode.    
  3.3 Read the object from the disk using the obj\_disk API into the allocated buffers
  3.4 Copy the buffers content according to given offset and size to the given dst buffer
  3.5 Free the allocated buffers
```

```
obj_cache_delete(dev, name):
1. Allocate all the buffers of the object being read, as "not-cached" buffers. 
2. Invalidate all the allocated buffers
3. Delete the object from disk using the "obj_disk"
4. API Free the allocated buffers
```

