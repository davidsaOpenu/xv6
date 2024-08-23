// Create a zombie process that
// must be reparented at exit.

#include "stat.h"
#include "types.h"
#include "lib/user.h"

int main(void) {
  if (fork() > 0) sleep(5);  // Let child exit before parent.
  exit(0);
}
