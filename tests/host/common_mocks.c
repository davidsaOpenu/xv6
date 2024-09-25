#include "common_mocks.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "framework/test.h"
#include "kernel/defs.h"
#include "kernel/mmu.h"
#include "kernel/sleeplock.h"
#include "spinlock.h"

static char g_memory[NUMBER_OF_PAGES][PGSIZE] = {0};
static int g_availability_index[NUMBER_OF_PAGES];
static int g_expect_panic = 0;
static int g_expect_panic_is_child = 0;

void init_mocks_environment() {
  g_expect_panic = 0;
  g_expect_panic_is_child = 0;

  // setup memory
  for (int i = 0; i < NUMBER_OF_PAGES; i++) {
    g_availability_index[i] = 1;
  }
}

int is_panic_handler_process() { return g_expect_panic_is_child; }

/* Expected panic()s are tested by creating a child process, which will run
 * the panic code and will signal the parent process whether panic was raised.
 * We cannot handle panic() and continue in the flow in the same process as
 * panic() is not expected to return.
 */
void start_expect_panic(void) {
  g_expect_panic = 1;

  pid_t fork_result = fork();
  if (-1 == fork_result) {
    FAIL_TEST("framework: failed to enter expect panic block");
  }

  if (0 == fork_result) {
    g_expect_panic_is_child = 1;
  }
}

void stop_expect_panic(void) {
  // If the child process reached here it means no panic was called.
  if (is_panic_handler_process()) {
    exit(EXIT_STATUS_EXPECTED_PANIC);
  } else {
    // Wait for the child process and get its exit code.
    int child_exit_code;

    wait(&child_exit_code);
    if (!WIFEXITED(child_exit_code)) {
      FAIL_TEST("framework: test was aborted unexpectedly");
    }
    if (EXIT_STATUS_EXPECTED_PANIC == WEXITSTATUS(child_exit_code)) {
      FAIL_TEST("expected a panic, which was not raised");
    }
    if (EXIT_STATUS_FAILURE == WEXITSTATUS(child_exit_code)) {
      FAIL_TEST("framework: test failed inside an expected panic block");
    }

    g_expect_panic = 0;
  }
}

void panic(char *msg) {
  if (!g_expect_panic) {
    char test_msg[512] = {0};
    snprintf(test_msg, sizeof(test_msg), "panic() was called with: \"%s\"",
             msg);
    FAIL_TEST(test_msg);
  } else if (is_panic_handler_process()) {
    // Signal the parent process a panic() was actually raised.
    exit(EXIT_STATUS_SUCCESS);
  } else {
    FAIL_TEST("framework: invalid usage of expect panic block");
  }
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

struct cgroup *proc_get_cgroup(void) { return 0; }

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
