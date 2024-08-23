// Create a zombie process that
// must be reparented at exit.

#include "../common/stat.h"
#include "../common/types.h"
#include "lib/user.h"

int main(void) {
  if (fork() > 0) sleep(5);  // Let child exit before parent.
  exit(0);
}
