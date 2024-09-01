#include "lib/user.h"
#include "stat.h"
#include "types.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf(stderr, "Usage: ln old new\n");
    exit(1);
  }
  if (link(argv[1], argv[2]) < 0)
    printf(stderr, "link %s %s: failed\n", argv[1], argv[2]);
  exit(0);
}
