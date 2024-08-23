#include "../common/stat.h"
#include "../common/types.h"
#include "lib/user.h"

int main(int argc, char **argv) {
  int i;

  if (argc < 2) {
    printf(2, "usage: kill pid...\n");
    exit(1);
  }
  for (i = 1; i < argc; i++) kill(atoi(argv[i]));
  exit(0);
}
