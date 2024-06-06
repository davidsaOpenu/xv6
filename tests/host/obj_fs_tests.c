#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_mocks.h"
#include "framework/test.h"
#include "kernel/buf.h"
#include "kernel/defs.h"
#include "kernel/device.h"
#include "kernel/obj_cache.h"
#include "kernel/obj_disk.h"

#define TESTED_DEVICE (0)

/**
 * Objfs mockings
 */
struct obj_device g_tested_dev = {0};

struct obj_device* objdeviceget(uint dev) { return &g_tested_dev; }

void deviceput(uint dev) {}

/**
 * Utility test functions
 */
static void copy_bufs_vector_to_buffer(char* buffer, vector bufs, uint size) {
  uint copied_bytes = 0;
  struct buf* curr_buf;

  if (0 < size) {
    for (uint buf_index = 0; buf_index < bufs.vectorsize; buf_index++) {
      uint block_size = min(BUF_DATA_SIZE, size - copied_bytes);
      memmove_from_vector((char*)&curr_buf, bufs, buf_index, 1);
      memmove(buffer + copied_bytes, curr_buf->data, block_size);
      copied_bytes += block_size;
    }
  }
}

static void copy_buffer_to_bufs_vector(vector bufs, const char* buffer,
                                       uint size) {
  uint copied_bytes = 0;
  struct buf* curr_buf;

  if (0 < size) {
    for (uint buf_index = 0; buf_index < bufs.vectorsize; buf_index++) {
      uint block_size = min(BUF_DATA_SIZE, size - copied_bytes);
      memmove_from_vector((char*)&curr_buf, bufs, buf_index, 1);
      memmove(curr_buf->data, buffer + copied_bytes, block_size);
      copied_bytes += block_size;
    }
  }
}

static void vector_bufs_memset(vector bufs, char c, uint size) {
  uint set_bytes = 0;
  struct buf* curr_buf;

  if (0 < size) {
    for (uint buf_index = 0; set_bytes < size; buf_index++) {
      uint block_size = min(BUF_DATA_SIZE, size - set_bytes);
      memmove_from_vector((char*)&curr_buf, bufs, buf_index, 1);
      memset(curr_buf->data, c, block_size);
      set_bytes += block_size;
    }
  }
}

static vector new_bufs_vector(struct buf* bufs, uint len) {
  vector bufs_vector = newvector(len, sizeof(struct buf*));
  struct buf* current_buf;

  for (uint i = 0; i < len; i++) {
    current_buf = (bufs + i);
    memmove_into_vector_elements(bufs_vector, i, (char*)&current_buf, 1);
  }

  return bufs_vector;
}

/**
 * Disk layer tests
 */

const uint initial_objects_table_bytes =
    INITIAL_OBJECT_TABLE_SIZE * sizeof(ObjectsTableEntry);

/**
 * Tests that the metadata values inside the super block are intiialized
 * correctly by calling the get functions.
 */
TEST(initialization) {
  EXPECT_UINT_EQ(2, occupied_objects(&g_tested_dev));
  EXPECT_UINT_EQ(STORAGE_DEVICE_SIZE, device_size(&g_tested_dev));
  EXPECT_UINT_EQ(sizeof(struct objsuperblock) + initial_objects_table_bytes,
                 occupied_bytes(&g_tested_dev));
}

/**
 * Validates the correctness of getting the super block object.
 */
TEST(super_block_object) {
  uint size;
  struct objsuperblock sb;
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(sizeof(sb))];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  ASSERT_NO_ERR(object_size(TESTED_DEVICE, SUPER_BLOCK_ID, &size));
  ASSERT_UINT_EQ(sizeof(sb), size);

  ASSERT_NO_ERR(get_object(TESTED_DEVICE, SUPER_BLOCK_ID, bufs_vec));
  copy_bufs_vector_to_buffer((char*)&sb, bufs_vec, sizeof(sb));

  EXPECT_UINT_EQ(STORAGE_DEVICE_SIZE, sb.storage_device_size);
  EXPECT_UINT_EQ(sizeof(struct objsuperblock), sb.objects_table_offset);
  EXPECT_UINT_EQ(2, sb.occupied_objects);
  EXPECT_UINT_EQ(sizeof(struct objsuperblock) + initial_objects_table_bytes,
                 sb.bytes_occupied);

  freevector(&bufs_vec);
}

/**
 * Validates the correctness of the objects table initial state by getting the
 * objects table object. We don't check the entries themsels because they are
 * checked by calling `get_object`. We only left the check that the rest of
 * the table is empty.
 */
TEST(table_object) {
  uint size;
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(initial_objects_table_bytes)];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  ASSERT_NO_ERR(object_size(TESTED_DEVICE, OBJECT_TABLE_ID, &size));
  ASSERT_UINT_EQ(initial_objects_table_bytes, size);

  ObjectsTableEntry table[INITIAL_OBJECT_TABLE_SIZE];
  ASSERT_NO_ERR(get_object(TESTED_DEVICE, OBJECT_TABLE_ID, bufs_vec));
  copy_bufs_vector_to_buffer((char*)&table, bufs_vec,
                             initial_objects_table_bytes);

  for (size_t i = 3; i < INITIAL_OBJECT_TABLE_SIZE; ++i) {
    EXPECT_FALSE(table[i].occupied);
  }

  EXPECT_TRUE(table[0].occupied);
  EXPECT_UINT_EQ(sizeof(struct objsuperblock), table[0].size);
  EXPECT_TRUE(table[1].occupied);
  EXPECT_UINT_EQ(initial_objects_table_bytes, table[1].size);
}

TEST(get_object_with_name_too_long) {
  uint size;
  EXPECT_UINT_EQ(
      OBJECT_NAME_TOO_LONG,
      object_size(TESTED_DEVICE, "012345678901234567890123456789", &size));
}

TEST(get_non_existing_object) {
  uint size;
  EXPECT_UINT_EQ(OBJECT_NOT_EXISTS,
                 object_size(TESTED_DEVICE, "non_existing", &size));
}

TEST(add_single_object) {
  char my_string[] = "my super amazing string";
  char read_data[sizeof(my_string)] = {0};
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(initial_objects_table_bytes)];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  copy_buffer_to_bufs_vector(bufs_vec, my_string, sizeof(my_string));
  ASSERT_NO_ERR(
      add_object(TESTED_DEVICE, "simple_string", bufs_vec, sizeof(my_string)));
  uint size;
  ASSERT_NO_ERR(object_size(TESTED_DEVICE, "simple_string", &size));
  ASSERT_UINT_EQ(sizeof(my_string), size);

  vector_bufs_memset(bufs_vec, 0, sizeof(my_string));
  ASSERT_NO_ERR(get_object(TESTED_DEVICE, "simple_string", bufs_vec));
  copy_bufs_vector_to_buffer(read_data, bufs_vec, sizeof(my_string));
  ASSERT_UINT_EQ(0, strcmp(read_data, my_string));
}

TEST(add_object_already_exist) {
  uint placeholder = 0;
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(sizeof(placeholder))];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  ASSERT_NO_ERR(add_object(TESTED_DEVICE, "already_exist_0", bufs_vec,
                           sizeof(placeholder)));
  ASSERT_UINT_EQ(OBJECT_EXISTS, add_object(TESTED_DEVICE, "already_exist_0",
                                           bufs_vec, sizeof(placeholder)));
}

TEST(delete_existing_object) {
  uint placeholder = 0;
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(sizeof(placeholder))];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  ASSERT_NO_ERR(
      add_object(TESTED_DEVICE, "delete_0", bufs_vec, sizeof(placeholder)));
  uint size;
  ASSERT_NO_ERR(object_size(TESTED_DEVICE, "delete_0", &size));
  ASSERT_NO_ERR(delete_object(TESTED_DEVICE, "delete_0"));
  ASSERT_UINT_EQ(OBJECT_NOT_EXISTS,
                 object_size(TESTED_DEVICE, "delete_0", &size));

  freevector(&bufs_vec);
}

TEST(delete_no_existing_object) {
  ASSERT_UINT_EQ(OBJECT_NOT_EXISTS,
                 delete_object(TESTED_DEVICE, "not_existing"));
}

TEST(rewrite_existing_object_with_shorter_data) {
  char first_string[] = "0123456789";
  char second_string[] = "abcdef";
  char read_data[sizeof(second_string)] = {0};
  struct buf bufs_first[SIZE_TO_NUM_OF_BUFS(sizeof(first_string))];
  vector bufs_first_vec = new_bufs_vector(bufs_first, ARRAY_LEN(bufs_first));
  struct buf bufs_second[SIZE_TO_NUM_OF_BUFS(sizeof(first_string))];
  vector bufs_second_vec = new_bufs_vector(bufs_second, ARRAY_LEN(bufs_second));

  copy_buffer_to_bufs_vector(bufs_first_vec, first_string,
                             sizeof(first_string));
  ASSERT_NO_ERR(add_object(TESTED_DEVICE, "rewrite_shorter", bufs_first_vec,
                           sizeof(first_string)));

  // sanity check
  uint size;
  ASSERT_NO_ERR(object_size(TESTED_DEVICE, "rewrite_shorter", &size));
  ASSERT_UINT_EQ(sizeof(first_string), size);

  // rewrite
  copy_buffer_to_bufs_vector(bufs_second_vec, second_string,
                             sizeof(second_string));
  ASSERT_NO_ERR(write_object(TESTED_DEVICE, "rewrite_shorter", bufs_second_vec,
                             sizeof(second_string)));

  // validate the new size and data
  ASSERT_NO_ERR(object_size(TESTED_DEVICE, "rewrite_shorter", &size));
  ASSERT_UINT_EQ(sizeof(second_string), size);

  vector_bufs_memset(bufs_second_vec, 0, sizeof(second_string));
  ASSERT_NO_ERR(get_object(TESTED_DEVICE, "rewrite_shorter", bufs_second_vec));
  copy_bufs_vector_to_buffer(read_data, bufs_second_vec, sizeof(second_string));
  ASSERT_UINT_EQ(0, strcmp(read_data, second_string));

  freevector(&bufs_first_vec);
  freevector(&bufs_second_vec);
}

TEST(rewrite_existing_object_with_longer_data) {
  char first_string[] = "0123456789";
  char second_string[] = "01234567890123456789";
  char read_data[sizeof(second_string)] = {0};
  struct buf bufs_first[SIZE_TO_NUM_OF_BUFS(sizeof(first_string))];
  vector bufs_first_vec = new_bufs_vector(bufs_first, ARRAY_LEN(bufs_first));
  struct buf bufs_second[SIZE_TO_NUM_OF_BUFS(sizeof(first_string))];
  vector bufs_second_vec = new_bufs_vector(bufs_second, ARRAY_LEN(bufs_second));

  copy_buffer_to_bufs_vector(bufs_first_vec, first_string,
                             sizeof(first_string));
  ASSERT_NO_ERR(add_object(TESTED_DEVICE, "rewrite_longer", bufs_first_vec,
                           sizeof(first_string)));

  // sanity check
  uint size;
  ASSERT_NO_ERR(object_size(TESTED_DEVICE, "rewrite_longer", &size));
  ASSERT_UINT_EQ(sizeof(first_string), size);

  // rewrite
  copy_buffer_to_bufs_vector(bufs_second_vec, second_string,
                             sizeof(second_string));
  ASSERT_NO_ERR(write_object(TESTED_DEVICE, "rewrite_longer", bufs_second_vec,
                             sizeof(second_string)));

  // validate the new size and data
  ASSERT_NO_ERR(object_size(TESTED_DEVICE, "rewrite_longer", &size));
  ASSERT_UINT_EQ(sizeof(second_string), size);

  vector_bufs_memset(bufs_second_vec, 0, sizeof(second_string));
  ASSERT_NO_ERR(get_object(TESTED_DEVICE, "rewrite_longer", bufs_second_vec));
  copy_bufs_vector_to_buffer(read_data, bufs_second_vec, sizeof(second_string));
  ASSERT_UINT_EQ(0, strcmp(read_data, second_string));

  freevector(&bufs_first_vec);
  freevector(&bufs_second_vec);
}

TEST(writing_multiple_objects) {
  const char* objects_data[3] = {"first data", "second data", "third data"};
  const char* objects_name[3] = {
      "writing multiple 1",
      "writing multiple 2",
      "writing multiple 3",
  };
  struct buf* bufs[3];
  vector buf_vecs[3];

  for (uint i = 0; i < 3; ++i) {
    uint bufs_num = SIZE_TO_NUM_OF_BUFS(strlen(objects_data[i]) + 1);
    bufs[i] = malloc(bufs_num * BUF_DATA_SIZE);
    ASSERT_TRUE(NULL != bufs[i]);
    buf_vecs[i] = new_bufs_vector(bufs[i], bufs_num);
  }

  for (uint i = 0; i < 3; ++i) {
    copy_buffer_to_bufs_vector(buf_vecs[i], objects_data[i],
                               strlen(objects_data[i]) + 1);
    ASSERT_NO_ERR(add_object(TESTED_DEVICE, objects_name[i], buf_vecs[i],
                             strlen(objects_data[i]) + 1));
  }
  for (uint i = 0; i < 3; ++i) {
    uint size;
    ASSERT_NO_ERR(object_size(TESTED_DEVICE, objects_name[i], &size));
    ASSERT_UINT_EQ(strlen(objects_data[i]) + 1, size);
    char* actual_data = (char*)malloc(size);
    ASSERT_TRUE(actual_data != NULL);

    vector_bufs_memset(buf_vecs[i], 0, sizeof(size));
    ASSERT_NO_ERR(get_object(TESTED_DEVICE, objects_name[i], buf_vecs[i]));
    copy_bufs_vector_to_buffer(actual_data, buf_vecs[i], size);
    ASSERT_EQ(strcmp(objects_data[i], actual_data), 0);
    free(actual_data);
  }

  for (uint i = 0; i < 3; ++i) {
    freevector(&buf_vecs[i]);
    free(bufs[i]);
  }
}

TEST(add_to_full_table) {
  // Fill the disk
  uint num_of_free_entries =
      INITIAL_OBJECT_TABLE_SIZE - occupied_objects(&g_tested_dev);
  char object_id[OBJECT_ID_LENGTH] = {};
  const ulong small_object_size = 1000;
  ulong object_size = 0;
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(small_object_size)];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));
  struct buf* last_obj_bufs = 0;

  for (uint i = 0; i < num_of_free_entries; ++i) {
    object_size = small_object_size;
    snprintf(object_id, sizeof(object_id), "objid_%u", i);

    // Set the last entry to occupy all left space
    if ((num_of_free_entries - 1) == i) {
      freevector(&bufs_vec);

      object_size = device_size(&g_tested_dev) - occupied_bytes(&g_tested_dev);
      last_obj_bufs =
          malloc(SIZE_TO_NUM_OF_BUFS(object_size) * sizeof(struct buf));
      ASSERT_NE(last_obj_bufs, 0);
      bufs_vec =
          new_bufs_vector(last_obj_bufs, SIZE_TO_NUM_OF_BUFS(object_size));
      vector_bufs_memset(bufs_vec, 4, object_size);
    }

    ASSERT_NO_ERR(add_object(TESTED_DEVICE, object_id, bufs_vec, object_size));
  }

  // Write again to the last object
  ASSERT_NO_ERR(write_object(TESTED_DEVICE, object_id, bufs_vec, object_size));

  freevector(&bufs_vec);
  free(last_obj_bufs);
  bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));
  ASSERT_UINT_EQ(OBJECTS_TABLE_FULL,
                 add_object(TESTED_DEVICE, "non existing object", bufs_vec,
                            small_object_size));
  freevector(&bufs_vec);
}

uint find_object_offset(const char* object_name) {
  ObjectsTableEntry table[INITIAL_OBJECT_TABLE_SIZE];
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(sizeof(table))];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  get_object(TESTED_DEVICE, OBJECT_TABLE_ID, bufs_vec);
  copy_bufs_vector_to_buffer((char*)table, bufs_vec, sizeof(table));

  uint address = -1;
  for (uint i = 0; i < INITIAL_OBJECT_TABLE_SIZE; ++i) {
    if (strcmp(table[i].object_id, object_name) == 0) {
      address = table[i].disk_offset;
      break;
    }
  }
  return address;
}

TEST(reusing_freed_space) {
  uint data = 0;
  struct buf bufs[SIZE_TO_NUM_OF_BUFS(sizeof(data))];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  ASSERT_NO_ERR(
      add_object(TESTED_DEVICE, "reusing object 1", bufs_vec, sizeof(data)));
  uint obj_1_offset = find_object_offset("reusing object 1");
  ASSERT_NE(obj_1_offset, -1);

  ASSERT_NO_ERR(delete_object(TESTED_DEVICE, "reusing object 1"));
  ASSERT_NO_ERR(
      add_object(TESTED_DEVICE, "reusing object 2", bufs_vec, sizeof(data)));
  uint obj_2_offset = find_object_offset("reusing object 2");
  ASSERT_UINT_EQ(obj_1_offset, obj_2_offset);

  freevector(&bufs_vec);
}

TEST(add_when_there_is_no_more_disk_left) {
  struct buf bufs[1];
  vector bufs_vec = new_bufs_vector(bufs, ARRAY_LEN(bufs));

  ASSERT_UINT_EQ(NO_DISK_SPACE_FOUND,
                 add_object(TESTED_DEVICE, "object too large", bufs_vec,
                            STORAGE_DEVICE_SIZE + 1));
}

// TODO(SM): Re-implement the objfs cache tests, according to the new caching
//           system
#if 0
/**
 * The following tests validate the correctness of the cache layer.
 * The tests use the `objects_cache_hits` and `objects_cache_misses` methods
 * to check the cache behavior vs the expected flow.
 */

TEST(get_object_in_cache) {
  char my_string[] = "my super amazing string";
  const char* obj_name = "get_object_in_cache";
  // inserting the object through the cache keeps it inside it
  cache_add_object(TESTED_DEVICE, my_string, strlen(my_string) + 1, obj_name);

  uint misses_at_start = objects_cache_misses();
  uint hits_at_start = objects_cache_hits();

  // validate correctness
  vector actual = newvector(1, sizeof(my_string));
  ASSERT_NO_ERR(cache_get_object(TESTED_DEVICE, name, &actual));
  ASSERT_UINT_EQ(0, vectormemcmp(actual, my_string, sizeof(my_string)));

  // validate hits and misses
  EXPECT_UINT_EQ(0, objects_cache_misses() - misses_at_start);
  EXPECT_UINT_EQ(1, objects_cache_hits() - hits_at_start);

  freevector(&actual);
}

TEST(get_object_not_in_cache) {
  char my_string[] = "my super amazing string";
  const char* obj_name = "object_no_cache_00";
  // inserting the object WITHOUT going through the cache
  ASSERT_NO_ERR(
      add_object(TESTED_DEVICE, my_string, sizeof(my_string), obj_name));

  uint misses_at_start = objects_cache_misses();
  uint hits_at_start = objects_cache_hits();

  // validate correctness
  vector actual = newvector(1, sizeof(my_string));
  ASSERT_NO_ERR(cache_get_object(TESTED_DEVICE, obj_name, &actual));
  ASSERT_UINT_EQ(0, vectormemcmp(actual, my_string, sizeof(my_string)));

  // validate hits and misses
  EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
  ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);

  // re-accessing the object now set "hit" because it's in the cache from
  // previous attempt.
  ASSERT_NO_ERR(cache_get_object(TESTED_DEVICE, obj_name, &actual));
  EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
  ASSERT_UINT_EQ(1, objects_cache_hits() - hits_at_start);

  freevector(&actual);
}

TEST(get_object_size_in_cache) {
  char my_string[] = "my super amazing string";
  const char* obj_name = "object_in_cache_01";
  // inserting the object WITH going through the cache
  cache_add_object(TESTED_DEVICE, my_string, strlen(my_string) + 1, obj_name);

  uint misses_at_start = objects_cache_misses();
  uint hits_at_start = objects_cache_hits();

  vector actual = newvector(1, sizeof(my_string));
  ASSERT_NO_ERR(cache_get_object(TESTED_DEVICE, obj_name, &actual));

  // validate hits and misses
  EXPECT_UINT_EQ(0, objects_cache_misses() - misses_at_start);
  ASSERT_UINT_EQ(1, objects_cache_hits() - hits_at_start);

  freevector(&actual);
}

TEST(get_object_size_not_in_cache_and_doesnt_add_to_cache) {
  char my_string[] = "my super amazing string";
  const char* obj_name = "object_no_cache_02";
  // inserting the object WITHOUT going through the cache
  add_object(TESTED_DEVICE, my_string, strlen(my_string) + 1, obj_name);

  uint misses_at_start = objects_cache_misses();
  uint hits_at_start = objects_cache_hits();

  // validate correctness
  uint size;
  ASSERT_NO_ERR(cache_object_size(TESTED_DEVICE, obj_name, &size));
  ASSERT_UINT_EQ(strlen(my_string) + 1, size);

  // validate hits and misses
  EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
  ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);

  ASSERT_NO_ERR(cache_object_size(TESTED_DEVICE, obj_name, &size));
  EXPECT_UINT_EQ(2, objects_cache_misses() - misses_at_start);
  ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);
}

TEST(object_too_large_not_inserted_to_cache) {
  uint large_data_size = cache_max_object_size() * 2;
  char* large_data = malloc(large_data_size);
  ASSERT_NE(large_data, 0);
  const char* obj_name = "object_no_cache_03";

  for (uint i = 0; i < sizeof(large_data); ++i) {
    large_data[i] = i % 256;
  }

  uint misses_at_start = objects_cache_misses();
  uint hits_at_start = objects_cache_hits();

  cache_add_object(TESTED_DEVICE, large_data, large_data_size, obj_name);

  // validate correctness
  vector actual = newvector(large_data_size, 1);
  ASSERT_NO_ERR(cache_get_object(TESTED_DEVICE, obj_name, &actual));

  ASSERT_UINT_EQ(0, vectormemcmp(actual, large_data, large_data_size));

  // validate hits and misses
  EXPECT_UINT_EQ(1, objects_cache_misses() - misses_at_start);
  ASSERT_UINT_EQ(0, objects_cache_hits() - hits_at_start);

  freevector(&actual);
}
#endif

INIT_TESTS_PLATFORM();

// Should be called before each test
void init_test() {
  init_mocks_environment();
  buf_cache_init();

  init_obj_device(TESTED_DEVICE);
}

int main() {
  SET_TEST_INITIALIZER(&init_test);

  // Driver layer
  run_test(initialization);
  run_test(super_block_object);
  run_test(table_object);
  run_test(add_single_object);
  run_test(add_object_already_exist);
  run_test(delete_existing_object);
  run_test(rewrite_existing_object_with_shorter_data);
  run_test(rewrite_existing_object_with_longer_data);
  run_test(writing_multiple_objects);
  run_test(get_object_with_name_too_long);
  run_test(get_non_existing_object);
  run_test(add_to_full_table);
  run_test(reusing_freed_space);
  run_test(add_when_there_is_no_more_disk_left);

  // TODO(SM): Re-implement the objfs cache tests, according to the new caching
  //           system
#if 0
  // Cache layer
  run_test(get_object_in_cache);
  run_test(get_object_not_in_cache);
  run_test(get_object_size_in_cache);
  run_test(get_object_size_not_in_cache_and_doesnt_add_to_cache);
  run_test(object_too_large_not_inserted_to_cache);
#endif

  PRINT_TESTS_RESULT("OBJ_FS_TESTS");
  return CURRENT_TESTS_RESULT();
}
