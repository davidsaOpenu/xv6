#include "common_mocks.h"

#include <stdarg.h>
#include <stdio.h>

#include "framework/test.h"
#include "kernel/defs.h"
#include "kernel/mmu.h"
#include "kernel/sleeplock.h"
#include "kernel/spinlock.h"

static char g_memory[NUMBER_OF_PAGES][PGSIZE] = {0};
static int g_availability_index[NUMBER_OF_PAGES];

void init_mocks_environment() {
  // setup memory
  for (int i = 0; i < NUMBER_OF_PAGES; i++) {
    g_availability_index[i] = 1;
  }
}

void panic(char *msg) {
  char test_msg[512] = {0};
  snprintf(test_msg, sizeof(test_msg), "panic() was called with: \"%s\"", msg);
  FAIL_TEST(test_msg);
}

void initsleeplock(struct sleeplock *lk, char *name) {
  // NOTE: no need in locks in tests as we run them in a single thread
  lk->locked = 0;
}

void acquiresleep(struct sleeplock *lk) {
  // NOTE: no need in locks in tests as we run them in a single thread
  lk->locked = 1;
}

void releasesleep(struct sleeplock *lk) {
  // NOTE: no need in locks in tests as we run them in a single thread
  lk->locked = 0;
}

int holdingsleep(struct sleeplock *lk) { return lk->locked; }

void initlock(struct spinlock *lk, char *name) {
  // NOTE: no need in locks in tests as we run them in a single thread
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

void acquire(struct spinlock *lk) {
  // NOTE: no need in locks in tests as we run them in a single thread
  lk->locked = 1;
}

int holding(struct spinlock *lk) {
  // NOTE: no need in locks in tests as we run them in a single thread
  return lk->locked;
}

void release(struct spinlock *lk) { lk->locked = 0; }

struct cgroup *proc_get_cgroup(void) {
  return 0;
}

void cgroup_mem_stat_pgfault_incr(struct cgroup *cgroup) {}

void cgroup_mem_stat_pgmajfault_incr(struct cgroup *cgroup) {}

char *kalloc() {
  for (int i = 0; i < NUMBER_OF_PAGES; i++) {
    if (g_availability_index[i] == 1) {
      g_availability_index[i] = 0;
      return g_memory[i];
    }
  }
  return NULL;
}

void kfree(char *ptr) {
  for (int i = 0; i < NUMBER_OF_PAGES; i++) {
    if (ptr == &g_memory[i][0]) {
      g_availability_index[i] = 0;
    }
  }
}

void cprintf(char *format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}
