#include "fcntl.h"
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "types.h"
#include "user.h"

static int cp(const char* src, const char* target);

static int join_paths(char* dest, int dest_size, const char* path1,
                      const char* path2) {
  int path1_len = strlen(path1);
  if (path1_len >= dest_size) return -1;
  memmove(dest, path1, path1_len);
  dest += path1_len;
  dest_size -= path1_len;

  if (dest[0] != '/') {
    if (dest_size <= 1) return -1;
    dest[0] = '/';
    dest++;
    dest_size--;
  }

  int path2_len = strlen(path2);
  if (path2_len >= dest_size) return -1;

  memmove(dest, path2, path2_len);
  dest += path2_len;

  *dest = '\0';
  return 0;
}

static int copy_file(int fd, const char* target) {
  int fdt, n;
  char buf[512];

  if ((fdt = open(target, O_CREATE | O_WRONLY)) < 0) {
    printf(2, "can't create %s, make sure the entire path exists\n", target);
    return -1;
  }

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(fdt, buf, n) != n) {
      printf(2, "cp: write error\n");
      return -1;
    }
  }

  return 0;
}

static int copy_dir(int fd, const char* src, const char* target) {
  /* Verify target is a directory or create it. */
  int target_fd = open(target, O_RDONLY);
  if (target_fd >= 0) {
    struct stat st;
    if (fstat(target_fd, &st) < 0) {
      printf(2, "cannot stat %s\n", target);
      close(target_fd);
      return -1;
    }

    if (st.type != T_DIR) {
      printf(2, "target %s is not a directory\n", target);
      close(target_fd);
      return -1;
    }
  } else if (mkdir(target) < 0) {
    return -1;
  }

  struct dirent de;
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;

    char name[DIRSIZ + 1];
    memmove(name, de.name, sizeof(de.name));
    name[DIRSIZ] = '\0';

    if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0)) continue;

    char joined_src[MAX_PATH_LENGTH];
    if (join_paths(joined_src, sizeof(joined_src), src, name) < 0) return -1;

    char joined_target[MAX_PATH_LENGTH];
    if (join_paths(joined_target, sizeof(joined_target), target, name) < 0)
      return -1;

    if (cp(joined_src, joined_target) < 0) return -1;
  }

  return 0;
}

static int cp(const char* src, const char* target) {
  int fd, err;
  struct stat st;

  if ((fd = open(src, O_RDONLY)) < 0) {
    printf(2, "can't locate %s\n", src);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    printf(2, "cannot stat %s\n", src);
    close(fd);
    return -1;
  }

  switch (st.type) {
    case T_FILE:
      err = copy_file(fd, target);
      break;
    case T_DIR:
      err = copy_dir(fd, src, target);
      break;
    default:
      err = -1;
      printf(2, "Unsupported file type %d\n", st.type);
      break;
  }

  close(fd);
  return err;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf(2, "cp requires two arguments: source and target\n");
    exit(1);
  }

  const char* src = argv[1];
  const char* target = argv[2];

  if (cp(src, target) < 0) exit(1);

  exit(0);
}
