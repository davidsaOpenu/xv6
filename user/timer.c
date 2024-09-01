#include "lib/user.h"
#include "types.h"

int main(int argc, char *argv[]) {
  int i = 0;
  while (1) {
    printf(stdout, "seconds: %d\n", i);
    usleep(1 * 1000 * 1000);
    ++i;
  }
  return 0;
}
