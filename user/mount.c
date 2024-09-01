#include "lib/user.h"  // NOLINT(build/include)

int main(int argc, const char* const argv[]) {
  const char usage[] =
      "Usage:\n"
      "mount [-t [fstype]] [path]\n"
      "mount [path] [-t [fstype]]\n"
      "mount -t bind path target_path\n"
      "mount internal_fs_{a|b|c} path\n";
  const char* fstype = 0;
  const char* path = 0;
  const char* bind = 0;
  int index = 0;

  if ((strcmp(argv[1], "internal_fs_a") == 0) ||
      (strcmp(argv[1], "internal_fs_b") == 0) ||
      (strcmp(argv[1], "internal_fs_c") == 0))
    exit(mount(argv[1], argv[2], 0));

  for (index = 1; index < argc;) {
    if (!strcmp(argv[index], "-t")) {
      ++index;
      if (index >= argc || fstype) {
        printf(stderr, usage);
        exit(1);
      }
      fstype = argv[index];
      ++index;
      continue;
    }

    if (path && bind) {
      printf(stderr, usage);
      exit(1);
    }

    if (!path) {
      path = argv[index];
      ++index;
    } else {
      bind = argv[index];
      ++index;
    }
  }

  if (bind && (!fstype || strcmp(fstype, "bind"))) {
    printf(stderr, usage);
    exit(1);
  }

  if (!path) {
    printf(stderr, usage);
    exit(1);
  }

  exit(mount(bind, path, fstype));
}
