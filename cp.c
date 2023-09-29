#include "fcntl.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "types.h"
#include "user.h"

char* fmtname(char* path) {
  char* p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--) {
  }
  p++;

  return p;
}

int copy_file(const char* src, const char* target) {
  int fds, fdt, n;
  char buf[512];
  if ((fds = open(src, O_RDONLY)) < 0) {
    printf(2, "cant locate %s\n", src);
    return -1;
  }
  if ((fdt = open(target, O_CREATE | O_WRONLY)) < 0) {
    printf(2, "cant create %s, make sure the entire path exists\n", target);
    return -1;
  }
  while ((n = read(fds, buf, sizeof(buf))) > 0) {
    if (write(fdt, buf, n) != n) {
      printf(1, "cp: write error\n");
      exit(1);
    }
  }
  return 0;
}

int cp(const char* src, const char* target) {
  int fd;
  struct stat st;
  struct dirent de;
  char buf[128], *p;

  if ((fd = open(src, O_RDONLY)) < 0) {
    printf(2, "cant locate %s\n", src);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    printf(2, "cannot stat %s\n", src);
    close(fd);
    return -1;
  }

  switch (st.type) {
    case T_FILE:
      close(fd);
      copy_file(src, target);
      break;

    case T_DIR:
      strcpy(buf, src);
      p = buf + strlen(buf) - 1;
      if (*p != '/') {
        p++;
        *p++ = '/';
      }
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf(1, "ls: cannot stat %s\n", buf);
          continue;
        }
        if ((strcmp(fmtname(buf), ".") == 0) ||
            (strcmp(fmtname(buf), "..") == 0)) {
          continue;
        }
        char new_target[128], *t;
        strcpy(new_target, target);
        t = new_target + strlen(new_target) - 1;
        if (*t != '/') {
          t++;
          *t++ = '/';
        }
        memmove(t, de.name, DIRSIZ);
        if (st.type == T_DIR) {
          mkdir(new_target);
        }
        cp(buf, new_target);
      }
      close(fd);
      break;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf(1, "cp requires two arguemnts, source and target\n");
    exit(1);
  }
  cp(argv[1], argv[2]);
  exit(0);
}
