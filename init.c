// init: The initial user-level program

#include "fcntl.h"
#include "stat.h"
#include "types.h"
#include "user.h"

char *argv[] = {"sh", 0};

static int init_procfs() {
  int procfs_fd = -1;
  // Check if procfs already created
  if ((procfs_fd = open("/proc", O_RDWR)) < 0) {
    if (mkdir("/proc") != 0) {
      printf(1, "init: failed to create root proc fs\n");
      return -1;
    }

    if (mount(0, "/proc", "proc") != 0) {
      printf(1, "init: failed to mount proc fs\n");
      return -1;
    }
  } else {
    if (close(procfs_fd) < 0) return -1;
  }

  return 0;
}

static int init_image_dir() {
  int image_dir_fd = -1;
  // Check if images directory already created
  if ((image_dir_fd = open(IMAGE_DIR, O_RDWR)) < 0) {
    if (mkdir(IMAGE_DIR) != 0) {
      printf(1, "init: failed to create images directory\n");
      return -1;
    }
  } else {
    if (close(image_dir_fd) < 0) return -1;
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

  mknod("tty0", 1, 1);
  mknod("tty1", 1, 2);
  mknod("tty2", 1, 3);

  if(init_image_dir() != 0) {
    printf(1, "init: init image_dir failed\n");
    exit(1);
  }

  if (init_procfs() != 0) {
    printf(1, "init: init procfs failed\n");
    exit(1);
  }

  for (;;) {
    printf(1, "init: starting sh\n");
    pid = fork();
    if (pid < 0) {
      printf(1, "init: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit(1);
    }
    while ((wpid = wait(0)) >= 0 && wpid != pid) printf(1, "zombie!\n");
  }
}
