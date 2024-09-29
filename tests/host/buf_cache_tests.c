#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_mocks.h"
#include "framework/test.h"
#include "kernel/defs.h"
#include "kernel/device/buf.h"
#include "kernel/device/buf_cache.h"
#include "param.h"

/* Verify buffers remain valid upon release. */
TEST(buffer_validity) {
  struct device tested_dev = {0};
  union buf_id id = {.blockno = 0};
  struct buf* buffer;

  buffer = buf_cache_get(&tested_dev, &id, 0);
  buffer->flags |= B_VALID;
  buf_cache_release(buffer);

  buffer = buf_cache_get(&tested_dev, &id, 0);
  EXPECT_TRUE(buffer->flags & B_VALID);
}

/* Verify used buffers not are allocated. */
TEST(no_used_allocated) {
  struct device tested_dev = {0};
  union buf_id id;

  /* Allocate all buffers and mark them dirty. */
  for (uint i = 0; i < NBUF; i++) {
    id.blockno = i;
    buf_cache_get(&tested_dev, &id, 0);
  }

  /* Try to allocate one more buffer and expect to fail since all are dirty. */
  EXPECT_PANIC(id.blockno = NBUF + 1; buf_cache_get(&tested_dev, &id, 0););
}

/* Verify dirty buffers are not allocated. */
TEST(no_dirty_allocated) {
  struct device tested_dev = {0};
  union buf_id id;
  struct buf* buffers[NBUF];

  /* Allocate all buffers and mark them dirty. */
  for (uint i = 0; i < NBUF; i++) {
    id.blockno = i;
    buffers[i] = buf_cache_get(&tested_dev, &id, 0);
    buffers[i]->flags |= B_DIRTY;
  }

  /* Free all buffers. */
  for (uint i = 0; i < NBUF; i++) {
    buf_cache_release(buffers[i]);
  }

  /* Try to allocate one more buffer and expect to fail since all are dirty. */
  EXPECT_PANIC(id.blockno = NBUF + 1; buf_cache_get(&tested_dev, &id, 0););
}

TEST(lru_mechanism) {
  struct device tested_dev = {0};
  struct device tested_dev2 = {0};
  uint seed = 0x1337;
  union buf_id id;
  struct buf* buffers[NBUF] = {0};

  /* Allocate all buffers. */
  for (uint i = 0; i < NBUF; i++) {
    id.blockno = i;
    buffers[i] = buf_cache_get(&tested_dev, &id, 0);
  }

  /* Mix the allocated buffers. */
  for (uint i = 0; i < 1000; i++) {
    uint first_index = rand_r(&seed) % NBUF;
    uint sec_index = rand_r(&seed) % NBUF;
    struct buf* tmp = buffers[first_index];
    buffers[first_index] = buffers[sec_index];
    buffers[sec_index] = tmp;
  }

  /* Free all buffers. */
  for (uint i = 0; i < NBUF; i++) {
    buf_cache_release(buffers[i]);
  }

  /* Allocate all buffers again and verify we get them according to the free()
   * order, thus validating the LRU policy. */
  for (uint i = 0; i < NBUF; i++) {
    // Allocate different buffers (not same device)
    id.blockno = i;
    struct buf* tmp = buf_cache_get(&tested_dev2, &id, 0);
    EXPECT_TRUE(tmp == buffers[i]);
  }
}

TEST(allocation_hint) {
  struct device tested_dev = {0};
  struct device tested_dev2 = {0};
  struct device tested_dev3 = {0};
  union buf_id id;
  struct buf* cached_buffers[NBUF / 2];
  struct buf* not_cached_buffers[NBUF / 2];

  /* Allocate and then free buffers with default caching hint. */
  for (uint i = 0; i < ARRAY_LEN(cached_buffers); i++) {
    id.blockno = i;
    cached_buffers[i] = buf_cache_get(&tested_dev, &id, 0);
  }
  for (uint i = 0; i < ARRAY_LEN(cached_buffers); i++) {
    buf_cache_release(cached_buffers[i]);
  }

  /* Allocate buffers with "no cache" hint. */
  for (uint i = 0; i < ARRAY_LEN(not_cached_buffers); i++) {
    id.blockno = i;
    not_cached_buffers[i] =
        buf_cache_get(&tested_dev2, &id, BUF_ALLOC_NO_CACHE);
  }
  for (uint i = 0; i < ARRAY_LEN(not_cached_buffers); i++) {
    buf_cache_release(not_cached_buffers[i]);
  }

  /* Allocate a new buffer and verify it is one of the "not cached" buffer,
   * even though they were released later. */
  id.blockno = 0;
  struct buf* new_buf = buf_cache_get(&tested_dev3, &id, 0);
  uint is_not_cached_buf = 0;
  for (uint i = 0; i < ARRAY_LEN(not_cached_buffers); i++) {
    if (new_buf == not_cached_buffers[i]) {
      is_not_cached_buf = 1;
    }
  }

  EXPECT_UINT_EQ(1, is_not_cached_buf);
}

INIT_TESTS_PLATFORM();

// Should be called before each test
void init_test() {
  init_mocks_environment();
  buf_cache_init();
}

int main() {
  SET_TEST_INITIALIZER(&init_test);

  run_test(buffer_validity);
  run_test(no_used_allocated);
  run_test(no_dirty_allocated);
  run_test(lru_mechanism);
  run_test(allocation_hint);

  PRINT_TESTS_RESULT("BUF_CACHE_TESTS");
  return CURRENT_TESTS_RESULT();
}
