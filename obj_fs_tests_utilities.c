#include "obj_fs_tests_utilities.h"

#include <stdio.h>
#include <stdlib.h>

void panic(const char* msg) {
  printf("kernel panic: %s\n", msg);
  exit(1);
}
