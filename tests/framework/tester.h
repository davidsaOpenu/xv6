#ifndef XV6_TESTER_H
#define XV6_TESTER_H

#include "include/wstatus.h"
#include "user/lib/user.h"

typedef int (*test_func_t)(void);
/* In case all tests passed, this value will remain 0,
 * else it will become 1. */
int testsPassed = 0;

int check(int r, const char *msg) {
  if (r < 0) {
    printf(stderr, "%s\n", (char *)msg);
    exit(1);
  }

  return r;
}

static int child_exit_status(int pid) {
  int changed_pid = -1;
  int wstatus;
  do {
    changed_pid = check(wait(&wstatus), "failed to waitpid");
  } while (changed_pid != pid);

  return WEXITSTATUS(wstatus);
}

/*This function runs the test, if a test fails, it will print which test failed,
and set the variable testsPassed to be 1*/
void run_test(test_func_t func, const char *name) {
  int pid = -1;
  static int tnum = 0;
  printf(stdout, "Running test %d: %s\n", tnum++, name);
  int ret = check(fork(), "fork failed");
  if (ret == 0) {
    exit(func());
  }

  pid = ret;
  if (child_exit_status(pid) != 0) { /*Test failed*/
    printf(stderr, "failed test - '%s'\n", name);
    testsPassed = 1; /*Denotes some test has failed to pass*/
  }
}
#endif /* XV6_TESTER_H */
