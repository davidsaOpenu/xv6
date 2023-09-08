#include "fcntl.h"
#include "fs.h"
#include "user.h"

int compare_files(const char *a, const char *b) {
  int a_fd = open(a, O_RDONLY);
  if (a_fd < 0) return 1;
  int b_fd = open(b, O_RDONLY);
  if (b_fd < 0) return 1;

  for (;;) {
    char c1, c2;
    int n1 = read(a_fd, &c1, sizeof(c1));
    int n2 = read(b_fd, &c2, sizeof(c2));
    if (n1 != n2) return 1;
    if (n1 == 0 && n2 == 0) return 0;
    if (c1 != c2) return 1;
  }

  close(a_fd);
  close(b_fd);
  return 0;
}

int compare_dirs(const char *lhs, const char *rhs, char **names,
                 int names_length) {
  for (int i = 0; i < names_length; i++) {
    char a[256];
    char b[256];
    strcpy(a, lhs);
    strcat(a, names[i]);
    strcpy(b, rhs);
    strcat(b, names[i]);
    if (compare_files(a, b) != 0) return 1;
  }
  return 0;
}
