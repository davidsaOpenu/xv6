#include "../common/fcntl.h"
#include "../common/fs.h"
#include "../common/param.h"
#include "../common/stat.h"
#include "lib/user.h"

#define RM_MAX_DEPTH (100)

static char paths_stack[RM_MAX_DEPTH][MAX_PATH_LENGTH] = {0};

static int is_dir(const char *path) {
  int fd, status;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    printf(2, "can't locate %s\n", path);
    return 0;
  }

  status = fstat(fd, &st);
  close(fd);
  if (status < 0) {
    printf(2, "cannot stat %s\n", path);
    return 0;
  }

  return T_DIR == st.type;
}

static int rm_file(const char *path) {
  int status;
  if (0 > (status = unlink(path))) printf(2, "cannot unlink %s\n", path);

  return status;
}

static int build_child_path(char *target, const char *parent,
                            struct dirent *child_entry) {
  uint path_len = strlen(parent);
  if ((path_len + strlen(child_entry->name) + 2) > MAX_PATH_LENGTH) {
    return -1;
  }
  strcpy(target, parent);
  target[path_len] = '/';
  strcpy(target + path_len + 1, child_entry->name);

  return 0;
}

// Remove the files in the given dir one after one,
// using Depth First Search
static int rm_dir_recursively(const char *dir_path) {
  int fd = -1, next_fd = -1;
  int stack_index = 0;
  struct dirent de = {0};

  strcpy(paths_stack[stack_index], dir_path);
  if (0 > (fd = open(dir_path, O_RDONLY))) {
    printf(2, "can't locate %s\n", dir_path);
    return -1;
  }

  // NOTE: We maintain 'fd' as the file of the currently checked path
  while (stack_index >= 0) {
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if ((de.inum == 0) || !strcmp(de.name, ".") || !strcmp(de.name, ".."))
        continue;

      if (stack_index == (RM_MAX_DEPTH - 1)) {
        printf(2, "reached max depth -- cannot rm %s/%s\n",
               paths_stack[RM_MAX_DEPTH - 1], de.name);
        return -1;
      }
      if (0 > (build_child_path(paths_stack[stack_index + 1],
                                paths_stack[stack_index], &de))) {
        printf(2, "cannot remove %s/%s -- path too long\n",
               paths_stack[stack_index], de.name);
        return -1;
      }
      if (!is_dir(paths_stack[stack_index + 1])) {
        if (0 > rm_file(paths_stack[stack_index + 1])) return -1;
      } else {
        // First remove the children entries
        if (0 > (next_fd = open(paths_stack[stack_index + 1], O_RDONLY))) {
          printf(2, "can't locate %s\n", paths_stack[stack_index + 1]);
          return -1;
        } else {
          close(fd);
          fd = next_fd;
          stack_index++;
        }
      }
    }

    // Now we can remove the dir itself
    close(fd);
    if (0 > rm_file(paths_stack[stack_index])) {
      return -1;
    }

    if (0 > (fd = open(paths_stack[--stack_index], O_RDONLY))) {
      printf(2, "can't locate %s\n", paths_stack[stack_index]);
      return -1;
    }
  }

  close(fd);

  return 0;
}

static int rm(const char *path, int recursive) {
  if ((!is_dir(path)) || !recursive) {
    return rm_file(path);
  }

  return rm_dir_recursively(path);
}

int main(int argc, char *argv[]) {
  int i = 0;
  int status = 0;
  int is_recursive = 0;
  int first_path_index = 1;

  if (argc < 2) {
    printf(2, "Usage: rm [-r] files...\n");
    exit(1);
  }

  if (!strcmp(argv[1], "-r")) {
    is_recursive = 1;
    first_path_index = 2;
  }

  for (i = first_path_index; i < argc; i++) {
    if (0 > rm(argv[i], is_recursive)) status = -1;
  }

  exit(status);
}
