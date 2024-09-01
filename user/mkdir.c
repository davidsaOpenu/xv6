#include "lib/user.h"
#include "stat.h"
#include "types.h"

int main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    printf(stderr, "Usage: mkdir files...\n");
    exit(1);
  }

  for (i = 1; i < argc; i++) {
    if (mkdir(argv[i]) < 0) {
      printf(stderr, "mkdir: %s failed to create\n", argv[i]);
      exit(1);
    }
  }

  exit(0);
}
