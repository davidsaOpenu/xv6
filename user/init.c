// init: The initial user-level program

#include "fcntl.h"
#include "lib/user.h"
#include "stat.h"
#include "types.h"

const char *argv[] = {"sh", 0};

// a function to check if a directory exists based on stat.h definitions
int dir_exists(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0 && st.type == T_DIR) {
    return 1;
  } else {
    return 0;
  }
}

static int init_procfs() {
  // Make sure the procfs mount point exists
  if (!dir_exists("/proc")) {
    if (mkdir("/proc") != 0) {
      printf(stdout, "init: failed to create procfs mount point\n");
      return -1;
    }
  }

  if (mount(0, "/proc", "proc") != 0) {
    printf(stdout, "init: failed to mount proc fs\n");
    return -1;
  }

  return 0;
}

static int init_image_dir() {
  // Make sure the images dir exists
  if (!dir_exists(IMAGE_DIR)) {
    if (mkdir(IMAGE_DIR) != 0) {
      printf(stdout, "init: failed to create images directory\n");
      return -1;
    }
  }

  return 0;
}

int main(void) {
  int pid, wpid;

  if (open("console", O_RDWR) < 0) {
    mknod("console", 1, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  mkdir(DEV_DIR);
  mknod(DEV_DIR "tty0", 1, 1);
  mknod(DEV_DIR "tty1", 1, 2);
  mknod(DEV_DIR "tty2", 1, 3);

  if (init_image_dir() != 0) {
    printf(stdout, "init: init image_dir failed\n");
    exit(1);
  }

  if (init_procfs() != 0) {
    printf(stdout, "init: init procfs failed\n");
    exit(1);
  }

  for (;;) {
    printf(stdout, "init: starting sh\n");
    pid = fork();
    if (pid < 0) {
      printf(stdout, "init: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      exec("sh", argv);
      printf(stdout, "init: exec sh failed\n");
      exit(1);
    }
    while ((wpid = wait(0)) >= 0 && wpid != pid) {
    }
  }
}
