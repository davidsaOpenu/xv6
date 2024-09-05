// Required for new gcc versions such as 9.4.0
#pragma GCC diagnostic ignored "-Wstack-usage="

#include "obj_disk.h"

#include "buf.h"
#include "defs.h"
#include "kvector.h"
#include "sleeplock.h"
#include "types.h"

#define entry_index_to_entry_offset(device, index) \
  device->sb.objects_table_offset + index * sizeof(ObjectsTableEntry)

struct objsuperblock super_block;

uint get_objects_table_index(struct obj_device* device, const char* name,
                             uint* output) {
  for (uint i = 0; i < get_object_table_size(device); ++i) {
    ObjectsTableEntry* entry = objects_table_entry(device, i);
    if (entry->occupied && obj_id_cmp(entry->object_id, name) == 0) {
      *output = i;
      return NO_ERR;
    }
  }

  return OBJECT_NOT_EXISTS;
}

ObjectsTableEntry* objects_table_entry(struct obj_device* device,
                                       uint entry_index) {
  return (ObjectsTableEntry*)&device
      ->memory_storage[entry_index_to_entry_offset(device, entry_index)];
}

uint flush_objects_table_entry(uint offset) {
  /**
   * Currently, because the implemntation is memory only, writing to the
   * entry received by `objects_table_entry` changes the table itself.
   * In the future, this method would write the specific bytes to the disk.
   */

  return NO_ERR;
}

int obj_id_cmp(const char* p, const char* q) {
  uint i = 0;
  while (*p && *p == *q && i < OBJECT_ID_LENGTH) {
    p++;
    q++;
    i++;
  }
  return (char)*p - (char)*q;
}

uint obj_id_bytes(const char* object_id) {
  uint bytes = strlen(object_id);
  if (bytes < MAX_OBJECT_NAME_LENGTH) {
    bytes++;  // null terminator as well.
  }
  return bytes;
}

void swap(uint* xp, uint* yp) {
  int temp = *xp;
  *xp = *yp;
  *yp = temp;
}

void bubble_sort(struct obj_device* device, uint* arr, uint n) {
  if (n >= 2) {
    for (uint i = 0; i < n - 1; i++) {
      for (uint j = 0; j < n - i - 1; j++) {
        if (objects_table_entry(device, arr[j])->disk_offset >
            objects_table_entry(device, arr[j + 1])->disk_offset) {
          swap(&arr[j], &arr[j + 1]);  // NOLINT(build/include_what_you_use)
        }
      }
    }
  }
}

/**
 * The method finds a sequence of empty bytes of length `size`.
 * If no such sequence exists, NULL is returned.
 *
 * To achieve this, the method first sort the objects in the table by their
 * address in the disk. Then, we check the space between every 2 consecutive
 * entries. We also check the space left between the end of the last element
 * and the end of the disk.
 * Notice that there are at least 2 objects in the table at every moment: the
 * super block and the objects table itself. Also, the super block is located
 * at address 0, hence we don't check the "empty space" before it because
 * there is no such space.
 *
 * Smarter implementation can be used in future work such as saving free
 * blocks in a list. Read "malloc internals" for details.
 */
static void* find_empty_space(struct obj_device* device, uint size) {
  // 1. put all occupied entries in an array
  uint current_table_size =
      min((device->sb.store_offset -  // NOLINT(build/include_what_you_use)
           device->sb.objects_table_offset),
          STORAGE_DEVICE_SIZE) /
      sizeof(ObjectsTableEntry);
  const uint entries_arr_size = current_table_size - 2;
  /* Be aware: Variable-length arrays are usually bad habit */
  uint entries_indices[entries_arr_size];  // NOLINT
  uint* current = entries_indices;
  uint populated_size = 0;
  for (uint i = 2; i < get_object_table_size(device); ++i) {
    if (objects_table_entry(device, i)->occupied) {
      *current = i;
      current++;
      populated_size++;
      if (current - entries_indices > entries_arr_size) {
        panic("found too much entries vs expected");
      }
    }
  }

  if (populated_size == 0 &&
      STORAGE_DEVICE_SIZE - size >= device->sb.store_offset) {
    return &device->memory_storage[STORAGE_DEVICE_SIZE - size];
  }

  uint last_occupied_entry = entries_indices[entries_arr_size - 1];
  // 2. sort it by objects disk-offset. smaller first.
  bubble_sort(device, entries_indices, populated_size);
  // 3. Check for rightmost spot
  ObjectsTableEntry* last_entry =
      objects_table_entry(device, entries_indices[populated_size - 1]);
  uint space_left = device->sb.storage_device_size - last_entry->disk_offset -
                    last_entry->size;
  if (space_left >= size) {
    return &device->memory_storage[STORAGE_DEVICE_SIZE - size];
  }
  // 4. for each occupied entry, measure space between this entry to the next.
  // if an appropriate space is found, return the rightmost memory chunk for
  // allocation.
  if (populated_size >= 2) {
    for (uint i = populated_size - 1; i > 0; i--) {
      ObjectsTableEntry* current_entry =
          objects_table_entry(device, entries_indices[i]);
      ObjectsTableEntry* prev_entry =
          objects_table_entry(device, entries_indices[i - 1]);
      space_left = current_entry->disk_offset - prev_entry->disk_offset -
                   prev_entry->size;
      if (space_left >= size) {
        return &device->memory_storage[current_entry->disk_offset - size];
      }
    }
  }
  // 5. could not find any space, consider moving the store space limit.
  ObjectsTableEntry* earliest_occupied_entry =
      objects_table_entry(device, entries_indices[0]);
  if (earliest_occupied_entry->disk_offset - device->sb.store_offset >= size) {
    return &device->memory_storage[earliest_occupied_entry->disk_offset - size];
  } else {
    uint needed_space =
        size - (earliest_occupied_entry->disk_offset - device->sb.store_offset);
    uint offset_after_last_entry =
        entry_index_to_entry_offset(device, last_occupied_entry + 1);
    if (offset_after_last_entry < device->sb.store_offset - needed_space) {
      device->sb.store_offset = device->sb.store_offset - needed_space;
      return &device->memory_storage[device->sb.store_offset];
    }
  }
  // 6. no solution without defragmentation.
  return NULL;
}

static void initialize_super_block_entry(struct obj_device* device) {
  ObjectsTableEntry* entry = objects_table_entry(device, 0);
  memmove(entry->object_id, SUPER_BLOCK_ID, strlen(SUPER_BLOCK_ID) + 1);
  entry->disk_offset = 0;
  entry->size = sizeof(device->sb);
  entry->occupied = 1;
}

static void initialize_objects_table_entry(struct obj_device* device) {
  ObjectsTableEntry* entry = objects_table_entry(device, 1);
  memmove(entry->object_id, OBJECT_TABLE_ID, strlen(OBJECT_TABLE_ID) + 1);
  entry->disk_offset = device->sb.objects_table_offset;
  entry->size = INITIAL_OBJECT_TABLE_SIZE * sizeof(ObjectsTableEntry);
  entry->occupied = 1;
}

// the disk lock should be held by the caller
static void write_super_block(struct obj_device* device) {
  memmove(device->memory_storage, &device->sb, sizeof(device->sb));
}

uint get_object_table_size(struct obj_device* device) {
  return (device->sb.store_offset - device->sb.objects_table_offset) /
         sizeof(ObjectsTableEntry);
}

void init_obj_device(uint dev) {
  struct obj_device* device = objdeviceget(dev);

  struct vfs_superblock sb;
  // with real device, we would read the block form the disk.
  initsleeplock(&device->disklock, "disklock");

  // Super block initializing
  device->sb.storage_device_size = STORAGE_DEVICE_SIZE;
  device->sb.objects_table_offset = sizeof(struct objsuperblock);
  device->sb.store_offset =
      device->sb.objects_table_offset +
      INITIAL_OBJECT_TABLE_SIZE * sizeof(ObjectsTableEntry);  // initial state
  device->sb.bytes_occupied =
      sizeof(device->sb) +
      INITIAL_OBJECT_TABLE_SIZE * sizeof(ObjectsTableEntry);
  device->sb.occupied_objects = 2;
  // Inode counter starts from 3, when 3 reserved to root dir object.
  device->sb.last_inode = 2;
  /* TODO(unknown): remove it? it is now meaningless since
   * inode number can grow and shrink. analyze
   * effect over vfs. */
  sb.ninodes = get_object_table_size(device);
  device->sb.vfs_sb = sb;
  // Inode initializing

  // To keep consistency, we write the super block to the disk and sets the
  // table state. This part should be read from the device and be created
  // when initializing the disk.
  for (uint i = 0; i < get_object_table_size(device); ++i) {
    objects_table_entry(device, i)->occupied = 0;
  }
  write_super_block(device);
  initialize_super_block_entry(device);
  initialize_objects_table_entry(device);

  deviceput(dev);
}

uint find_space_and_populate_entry(struct obj_device* device,
                                   ObjectsTableEntry* entry, const void* object,
                                   const char* name, uint size) {
  void* address = find_empty_space(device, size);
  if (!address) {
    return NO_DISK_SPACE_FOUND;
  }
  memmove(entry->object_id, name, obj_id_bytes(name));
  entry->disk_offset = address - (void*)device->memory_storage;
  entry->size = size;
  memmove(address, object, size);
  entry->occupied = 1;
  device->sb.bytes_occupied += size;
  device->sb.occupied_objects += 1;
  write_super_block(device);

  return NO_ERR;
}

uint add_object(uint dev, const void* object, uint size, const char* name) {
  uint err = NO_ERR;
  struct obj_device* device = objdeviceget(dev);

  // 1. check if the object is already present in disk
  err = check_add_object_validity(device, size, name);
  if (err != NO_ERR) {
    goto put_dev;
  }

  // 2. find first unoccupied entry of the objects table
  // then occupy it and allocate space for the new object.
  acquiresleep(&device->disklock);
  uint leftmost_disk_allocation_offset = STORAGE_DEVICE_SIZE;
  // We start to search after the superblock and entries table,
  // minus 1 since indexing starts in 0
  uint i;
  for (i = OBJ_ROOTINO - 1; i < get_object_table_size(device); i++) {
    ObjectsTableEntry* entry = objects_table_entry(device, i);
    if (entry->disk_offset < leftmost_disk_allocation_offset)
      leftmost_disk_allocation_offset = entry->disk_offset;
    if (!entry->occupied) {
      err = find_space_and_populate_entry(device, entry, object, name, size);
      goto unlock;
    }
  }
  // 3. all entries are occupied. is it possible to extend the table?
  // find offset of the first object.
  if (leftmost_disk_allocation_offset - device->sb.store_offset >=
      sizeof(ObjectsTableEntry)) {
    device->sb.store_offset =
        device->sb.store_offset + sizeof(ObjectsTableEntry);
    device->sb.bytes_occupied += sizeof(ObjectsTableEntry);
    ObjectsTableEntry* entry = objects_table_entry(device, i);
    err = find_space_and_populate_entry(device, entry, object, name, size);
    goto unlock;
  }

  err = OBJECTS_TABLE_FULL;

unlock:
  releasesleep(&device->disklock);
put_dev:
  deviceput(dev);

  return err;
}

uint rewrite_object(uint dev, vector data, uint objectsize,
                    uint write_starting_offset, const char* name) {
  uint err;
  struct obj_device* device = objdeviceget(dev);

  // 1. check for name contraints validity
  err = check_rewrite_object_validality(objectsize, name);
  if (err != NO_ERR) {
    goto put_dev;
  }
  // 2. name is ok. get the index off the object's entry
  acquiresleep(&device->disklock);
  uint i;
  err = get_objects_table_index(device, name, &i);
  if (err != NO_ERR) {
    goto unlock;
  }
  ObjectsTableEntry* entry = objects_table_entry(device, i);
  device->sb.bytes_occupied -= entry->size;
  if (entry->size >= objectsize) {
    // 3.A - the new object written is smaller or equals the the original.
    void* address = (void*)((uint)device->memory_storage + entry->disk_offset +
                            write_starting_offset);
    /* TODO(unknown)? instead of data.vectorsize
     * add parameter 'datasize' */
    memmove_from_vector(address, data, 0, data.vectorsize);
    entry->size = objectsize;
  } else {
    // 3.B - the new object is larger
    entry->occupied = 0;
    device->sb.occupied_objects -= 1;
    void* new_object_address = find_empty_space(device, objectsize);
    void* data_destination_address =
        (void*)((uint)new_object_address + write_starting_offset);
    entry->occupied = 1;
    device->sb.occupied_objects += 1;
    if (!new_object_address) {
      err = NO_DISK_SPACE_FOUND;
      goto unlock;
    }
    memmove(new_object_address,
            (void*)((uint)device->memory_storage + entry->disk_offset),
            entry->size);
    memmove_from_vector(data_destination_address, data, 0, data.vectorsize);
    entry->size = objectsize;
    entry->disk_offset = new_object_address - (void*)device->memory_storage;
  }
  device->sb.bytes_occupied += objectsize;
  write_super_block(device);
  err = NO_ERR;

unlock:
  releasesleep(&device->disklock);
put_dev:
  deviceput(dev);

  return err;
}

uint object_size(uint dev, const char* name, uint* output) {
  uint err = NO_ERR;
  struct obj_device* device = objdeviceget(dev);

  if (strlen(name) > MAX_OBJECT_NAME_LENGTH) {
    err = OBJECT_NAME_TOO_LONG;
    goto put_dev;
  }
  acquiresleep(&device->disklock);
  uint i;
  err = get_objects_table_index(device, name, &i);
  if (err != NO_ERR) {
    goto unlock;
  }
  ObjectsTableEntry* entry = objects_table_entry(device, i);
  *output = entry->size;
  err = NO_ERR;

unlock:
  releasesleep(&device->disklock);
put_dev:
  deviceput(dev);

  return err;
}

uint get_object(uint dev, const char* name, void* output,
                vector* outputvector) {
  uint err = NO_ERR;
  struct obj_device* device = objdeviceget(dev);

  // 1. make sure the name is of legal length
  if (strlen(name) > MAX_OBJECT_NAME_LENGTH) {
    err = OBJECT_NAME_TOO_LONG;
    goto put_dev;
  }
  // 2. try to locate the object in the object-table
  // return an index i or an error code
  acquiresleep(&device->disklock);
  uint i;
  err = get_objects_table_index(device, name, &i);
  if (err != NO_ERR) {
    goto unlock;
  }
  // 3. read the objects offset in disk, then read the object into
  // output address and vector
  ObjectsTableEntry* entry = objects_table_entry(device, i);
  void* address = (void*)((uint)device->memory_storage + entry->disk_offset);
  if (output != NULL) memmove(output, address, entry->size);
  if (outputvector != NULL)
    memmove_into_vector_bytes(*outputvector, 0, address, entry->size);
  err = NO_ERR;

unlock:
  releasesleep(&device->disklock);
put_dev:
  deviceput(dev);

  return err;
}

uint delete_object(uint dev, const char* name) {
  uint err = NO_ERR;
  struct obj_device* device = objdeviceget(dev);

  err = check_delete_object_validality(name);
  if (err != NO_ERR) {
    goto put_dev;
  }
  acquiresleep(&device->disklock);
  uint i;
  err = get_objects_table_index(device, name, &i);
  if (err != NO_ERR) {
    goto unlock;
  }
  ObjectsTableEntry* entry = objects_table_entry(device, i);
  entry->occupied = 0;
  device->sb.occupied_objects -= 1;
  device->sb.bytes_occupied -= entry->size;
  write_super_block(device);
  err = NO_ERR;

unlock:
  releasesleep(&device->disklock);
put_dev:
  deviceput(dev);

  return err;
}

uint check_add_object_validity(struct obj_device* device, uint size,
                               const char* name) {
  // currently, because we don't use hash function, we must first scan the
  // table and check if the object already exists.
  if (strlen(name) > MAX_OBJECT_NAME_LENGTH) {
    return OBJECT_NAME_TOO_LONG;
  }
  if (STORAGE_DEVICE_SIZE < size) {
    return NO_DISK_SPACE_FOUND;
  }
  for (uint i = 0; i < get_object_table_size(device); ++i) {
    if (objects_table_entry(device, i)->occupied &&
        obj_id_cmp(objects_table_entry(device, i)->object_id, name) == 0) {
      return OBJECT_EXISTS;
    }
  }
  return NO_ERR;
}

uint check_rewrite_object_validality(uint size, const char* name) {
  if (strlen(name) > MAX_OBJECT_NAME_LENGTH) {
    return OBJECT_NAME_TOO_LONG;
  }
  if (STORAGE_DEVICE_SIZE < size) {
    return NO_DISK_SPACE_FOUND;
  }
  return NO_ERR;
}

uint check_delete_object_validality(const char* name) {
  if (strlen(name) > MAX_OBJECT_NAME_LENGTH) {
    return OBJECT_NAME_TOO_LONG;
  }
  return NO_ERR;
}

uint new_inode_number(uint dev) {
  struct obj_device* device = objdeviceget(dev);
  uint new_inode;

  acquiresleep(&device->disklock);
  new_inode = ++device->sb.last_inode;
  write_super_block(device);
  releasesleep(&device->disklock);

  deviceput(dev);
  return new_inode;
}

uint occupied_objects(struct obj_device* device) {
  uint occupied_objects = 0;

  acquiresleep(&device->disklock);
  occupied_objects = device->sb.occupied_objects;
  releasesleep(&device->disklock);

  return occupied_objects;
}

void set_occupied_objects(struct obj_device* device, uint value) {
  int acquired = 0;
  if (!holdingsleep(&device->disklock)) {
    acquired = 1;
    acquiresleep(&device->disklock);
  }

  device->sb.occupied_objects = value;
  write_super_block(device);

  if (acquired) {
    releasesleep(&device->disklock);
  }
}

void set_store_offset(struct obj_device* device, uint new_offset) {
  int acquired = 0;
  if (!holdingsleep(&device->disklock)) {
    acquired = 1;
    acquiresleep(&device->disklock);
  }
  device->sb.store_offset = new_offset;
  write_super_block(device);
  if (acquired) {
    releasesleep(&device->disklock);
  }
}

uint device_size(struct obj_device* device) {
  uint size = 0;
  int acquired = 0;

  if (!holdingsleep(&device->disklock)) {
    acquired = 1;
    acquiresleep(&device->disklock);
  }

  size = device->sb.storage_device_size;

  if (acquired) {
    releasesleep(&device->disklock);
  }

  return size;
}

uint occupied_bytes(struct obj_device* device) {
  uint bytes = 0;

  acquiresleep(&device->disklock);
  bytes = device->sb.bytes_occupied;
  releasesleep(&device->disklock);

  return bytes;
}
