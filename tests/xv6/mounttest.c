#include "fcntl.h"
#include "framework/tester.h"
#include "fsdefs.h"
#include "kernel/memlayout.h"
#include "ns_types.h"
#include "param.h"
#include "stat.h"
#include "syscall.h"
#include "traps.h"
#include "types.h"
#include "user/lib/user.h"

static int createfile(const char *const path, const char *const contents) {
  int fd;
  if ((fd = open(path, O_WRONLY | O_CREATE)) < 0) {
    printf(stdout, "createfile: cannot open %s\n", path);
    return -1;
  }

  if (write(fd, contents, strlen(contents)) < 0) {
    printf(stdout, "createfile: failed writing\n", path);
    close(fd);
    return -1;
  }

  close(fd);

  return 0;
}

static int verifyfilecontents(char *path, char *contents) {
  int fd;
  struct stat st;
  if ((fd = open(path, 0)) < 0) {
    printf(stdout, "verifyfilecontents: cannot open %s\n", path);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    printf(stdout, "verifyfilecontents: cannot stat %s\n", path);
    close(fd);
    return -1;
  }

  int contentlen = strlen(contents);

  if (st.size != contentlen) {
    printf(stdout, "verifyfilecontents: incorrect length (%d) for file %s\n",
           st.size, path);
    close(fd);
    return -1;
  }

  char buf[100];
  int res;
  if ((res = read(fd, buf, contentlen)) != contentlen) {
    printf(stdout,
           "verifyfilecontents: incorrect length read (%d) for file %s\n", res,
           path);
    close(fd);
    return -1;
  }
  buf[contentlen] = '\0';
  close(fd);

  if ((res = strcmp(contents, buf)) != 0) {
    printf(stdout,
           "verifyfilecontents: incorrect content read (%s) for file %s\n", buf,
           path);
    return -1;
  }

  return 0;
}

static int countlines(char *path, int *lines) {
  int fd;
  int res;
  char buf[100] = {0};
  const char *bufp;
  int count = 0;

  if ((fd = open(path, 0)) < 0) {
    printf(stdout, "countlines: cannot open %s\n", path);
    return -1;
  }

  while ((res = read(fd, buf, sizeof(buf) - 1)) > 0) {
    bufp = (const char *)&buf;
    while ((bufp = strchr(bufp, '\n')) != 0) {
      count++;
      bufp++;
    }
    memset(buf, 0, sizeof(buf));
  }

  close(fd);
  *lines = count;

  return 0;
}

static int verifylines(char *path, int expected) {
  int lines = 0;
  if (countlines(path, &lines) != 0) {
    printf(stdout, "verifylines: failed to count lines of %s\n", path);
    return -1;
  }

  if (lines != expected) {
    printf(stdout,
           "verifylines: %s - expected %d lines, "
           "read %d lines\n",
           path, expected, lines);
    return -1;
  }

  return 0;
}

static int testfile(char *path) {
  if (createfile(path, "aaa") != 0) {
    return -1;
  }

  if (verifyfilecontents(path, "aaa") != 0) {
    return -1;
  }

  return 0;
}

static int mounta(void) {
  mkdir("a");
  int res = mount("internal_fs_a", "a", 0);
  if (res != 0) {
    printf(stdout, "mounta: mount returned %d\n", res);
    return -1;
  }

  return 0;
}

static int umounta(void) {
  int res = umount("a");
  if (res != 0) {
    printf(stdout, "umounta: umount returned %d\n", res);
    return -1;
  }

  return 0;
}

static int mounttest(void) {
  if (mounta() != 0) {
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int statroottest(void) {
  int pid = fork();

  if (pid < 0) {
    return 1;  // exit on error in fork
  }

  if (pid == 0) {
    // in child, only mount
    if (mounta() != 0) {
      return 1;
    }
    return 0;
  }

  int ret_val = child_exit_status(pid);  // get child exit status
  if (ret_val != 0) {
    return 1;
  }

  struct stat st;
  stat("a", &st);
  if (st.type != T_DIR || st.ino != 1 || st.size != BSIZE) {
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }
  return 0;
}

static int writefiletest(void) {
  if (mounta() != 0) {
    return 1;
  }

  if (testfile("a/test1") != 0) {
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int invalidpathtest(void) {
  int res = mount("internal_fs_a", "AAA", 0);
  if (res != -1) {
    printf(stdout, "invalidpathtest: mount did not fail as expected %d\n", res);
    return 1;
  }

  if (mounta() != 0) {
    return 1;
  }

  res = umount("b");
  if (res != -1) {
    printf(stdout, "invalidpathtest: umount did not fail as expected %d\n",
           res);
    return 1;
  }

  mkdir("b");
  res = umount("b");
  if (res != -1) {
    printf(stdout, "invalidpathtest: umount did not fail as expected %d\n",
           res);
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int doublemounttest(void) {
  if (mounta() != 0) {
    return 1;
  }

  mkdir("b");
  int res = mount("internal_fs_a", "b", 0);
  if (res != 0) {
    printf(stdout, "doublemounttest: mount returned %d\n", res);
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  res = umount("b");
  if (res != 0) {
    printf(stdout, "doublemounttest: umount returned %d\n", res);
    return 1;
  }

  return 0;
}

static int samedirectorytest(void) {
  if (mounta() != 0) {
    return 1;
  }

  int res = mount("internal_fs_b", "a", 0);
  if (res != -1) {
    printf(stdout, "samedirectorytest: mount did not fail as expected %d\n",
           res);
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int directorywithintest(void) {
  if (mounta() != 0) {
    return 1;
  }

  mkdir("a/ttt");
  if (testfile("a/ttt/test1") != 0) {
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int nestedmounttest(void) {
  if (mounta() != 0) {
    return 1;
  }

  mkdir("a/b");
  int res = mount("internal_fs_b", "a/b", 0);
  if (res != 0) {
    printf(stdout, "nestedmounttest: mount returned %d\n", res);
    return 1;
  }

  // Test access to a/b/test1 via different combinations and parent directories.
  if (testfile("a/b/test1") != 0 || testfile("a/b/../b/test1") != 0 ||
      testfile("a/../a/b/test1") != 0 || testfile("a/../b/../b/test1") != 0 ||
      testfile("a/../../a/b/../../a/b/test1") != 0 ||
      testfile("a/b/../b/../../a/b/test1") != 0 ||
      testfile("a/b/../../../../a/../a/../a/b/test1") != 0) {
    return 1;
  }

  res = umount("a");
  if (res != -1) {
    printf(stdout, "nestedmounttest: umount did not fail as expected %d\n",
           res);
    return 1;
  }

  res = umount("a/b");
  if (res != 0) {
    printf(stdout, "nestedmounttest: umount returned %d\n", res);
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int devicefilestoretest(void) {
  if (mounta() != 0) {
    return 1;
  }

  if (createfile("a/devicefilestoretest", "ababab") != 0) {
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  mkdir("ccc");
  int res = mount("internal_fs_a", "ccc", 0);
  if (res != 0) {
    printf(stdout, "devicefilestoretest: mount returned %d\n", res);
    return 1;
  }

  if (verifyfilecontents("ccc/devicefilestoretest", "ababab") != 0) {
    return 1;
  }

  res = umount("ccc");
  if (res != 0) {
    printf(stdout, "devicefilestoretest: umount did not fail as expected %d\n",
           res);
    return 1;
  }

  unlink("ccc");

  return 0;
}

static int umountwithopenfiletest(void) {
  if (mounta() != 0) {
    return 1;
  }

  int fd;
  if ((fd = open("a/umountwithop", O_WRONLY | O_CREATE)) < 0) {
    printf(stdout, "umountwithopenfiletest: cannot open file\n");
    return 1;
  }

  int res = umount("a");
  if (res != -1) {
    printf(stdout,
           "umountwithopenfiletest: umount did not fail as expected %d\n", res);
    return 1;
  }

  close(fd);

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int errorondeletedevicetest(void) {
  if (mounta() != 0) {
    return 1;
  }

  int res = unlink("internal_fs_a");
  if (res != -1) {
    printf(stdout,
           "errorondeletedevicetest: unlink did not fail as expected %d\n",
           res);
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  return 0;
}

static int umountnonrootmount(void) {
  mkdir("a");

  if (verifylines("/proc/mounts", 1) != 0) {
    printf(stdout, "umountnonrootmount: expected a single mount\n");
    return 1;
  }

  int res = mount(0, "a", "objfs");
  if (res != 0) {
    printf(stdout, "umountnonrootmount: mount returned %d\n", res);
    return -1;
  }

  if (verifylines("/proc/mounts", 2) != 0) {
    printf(stdout, "umountnonrootmount: expected two mounts\n");
    return 1;
  }

  res = umount("a");
  if (res != 0) {
    printf(stdout, "umountnonrootmount: umount returned %d\n", res);
    return -1;
  }

  if (verifylines("/proc/mounts", 1) != 0) {
    printf(stdout, "umountnonrootmount: expected a single mount\n");
    return 1;
  }

  return 0;
}

static int namespacetest(void) {
  if (mounta() != 0) {
    return 1;
  }

  int pid = fork();
  if (pid == 0) {
    unshare(MOUNT_NS);

    umounta();

    exit(0);
  } else {
    wait(0);
    if (umounta() != 0) {
      return 1;
    }

    return 0;
  }
}

static int namespacefiletest(void) {
  if (mounta() != 0) {
    return 1;
  }

  int pid = fork();
  if (pid == 0) {
    unshare(MOUNT_NS);
    mkdir("b");
    int res = mount("internal_fs_b", "b", 0);
    if (res != 0) {
      printf(stdout, "namespacefiletest: mount returned %d\n", res);
      return 1;
    }

    createfile("b/nsfiletest", "aaa");
    exit(0);
  } else {
    wait(0);

    if (umounta() != 0) {
      return 1;
    }
    if (open("b/nsfiletest", 0) >= 0) {
      printf(stdout,
             "namespacefiletest: should not have been able to open file\n");
      return 1;
    }

    int res = mount("internal_fs_b", "b", 0);
    if (res != 0) {
      printf(stdout, "namespacefiletest: mount returned %d\n", res);
      return 1;
    }

    int fd;
    if ((fd = open("b/nsfiletest", 0)) < 0) {
      printf(stdout, "namespacefiletest: failed to open file after mount\n");
      return 1;
    }

    close(fd);

    res = umount("b");
    if (res != 0) {
      printf(stdout, "namespacefiletest: umount returned %d\n", res);
      return 1;
    }

    return 0;
  }
}

static int cdinthenouttest(void) {
  if (mounta() != 0) {
    return 1;
  }

  chdir("a");
  chdir("..");

  // tmp replacment of pwd - checking if the wd contains the "a" dir
  struct stat st;
  if (stat("a", &st) < 0) {
    printf(stdout, "cdinthenouttest: not in root or couldnt find a dir\n");
    return 1;
  }

  int res = umount("a");
  if (res != 0) {
    printf(stdout, "cdinthenouttest: unmount returned %d\n", res);
    return 1;
  }
  return 0;
}

static int procfiletest(char *func_name, char *path, int initial_lines_count) {
  if (verifylines(path, initial_lines_count) != 0) {
    printf(stdout, "%s: failed to verify lines for %s\n", func_name, path);
    return 1;
  }

  if (mounta() != 0) {
    return 1;
  }

  if (verifylines(path, initial_lines_count + 1) != 0) {
    printf(stdout, "%s: failed to verify lines for %s\n", path, func_name);
    return 1;
  }

  if (umounta() != 0) {
    return 1;
  }

  if (verifylines(path, initial_lines_count) != 0) {
    printf(stdout, "%s: failed to verify lines for %s\n", path, func_name);
    return 1;
  }

  return 0;
}

static int procmountstest(void) {
  return procfiletest("procmountstest", "/proc/mounts", 1);
}

static int procdevicestest(void) {
  return procfiletest("procdevicestest", "/proc/devices", 0);
}

static int make_sure_dir_doesnt_exist(const char *const path) {
  struct stat st = {0};
  if (stat(path, &st) == 0) {
    if (unlink(path) != 0) {
      printf(stdout, "make_sure_dir_doesnt_exist: failed to unlink %s\n", path);
      return 1;
    }
    if (stat(path, &st) == 0) {
      printf(stdout, "make_sure_dir_doesnt_exist: failed to remove %s\n", path);
      return 1;
    }
  }
  return 0;
}

static int pivotrootfiletest(void) {
  struct stat testfile_stat;

  if (make_sure_dir_doesnt_exist("/a/oldroot") != 0 ||
      make_sure_dir_doesnt_exist("/a") != 0) {
    return 1;
  }

  // Mount fs under root dir
  if (mkdir("/a") < 0) {
    printf(stdout, "pivotrootfiletest: failed to create root dir\n");
    return 1;
  }
  int res = mount("internal_fs_a", "/a", 0);
  if (res != 0) {
    printf(stdout, "pivotrootfiletest: mount returned %d\n", res);
    return 1;
  }

  if (createfile("/test.txt", "in root mount") != 0) {
    printf(stdout, "pivotrootfiletest: failed to create test file\n");
    return 1;
  }

  // Create a dir for old root
  mkdir("/a/oldroot");
  if (pivot_root("/a", "/a/oldroot") != 0) {
    printf(stdout, "pivotrootfiletest: failed to pivot root!\n");
    return 1;
  }

  // Old root test file shouldn't exist in this path
  if (stat("/test.txt", &testfile_stat) == 0) {
    printf(stdout,
           "pivotrootfiletest: test file still exists in root directory\n");
    return 1;
  }

  // Old root test file should exist in this path
  if (stat("/oldroot/test.txt", &testfile_stat) != 0) {
    printf(stdout,
           "pivotrootfiletest: failed to find test file in old root dir\n");
    return 1;
  }

  // Start rollback to old root mount
  mkdir("/oldroot/a");
  if (pivot_root("/oldroot", "/oldroot/a") != 0) {
    printf(stdout,
           "pivotrootfiletest: failed to pivot root back to original root!\n");
    return 1;
  }

  if (stat("/test.txt", &testfile_stat) != 0) {
    printf(stdout,
           "pivotrootfiletest: failed to find test file in original root after "
           "pivot back to root\n");
    return 1;
  }

  // Cleanup of test mount for pivot_root
  if (umount("/a") != 0) {
    printf(stdout,
           "pivotrootfiletest: failed to umount new root for cleanup\n");
    return 1;
  }

  return 0;
}

// Test we can access bind mount from a umounted old root mount
static int pivotrootmounttest(void) {
  struct stat testfile_stat;
  int pid = fork();
  static const char *testfile = "/b/test.txt";
  if (pid == 0) {
    if (unshare(MOUNT_NS) != 0) {
      printf(stdout, "pivotrootmounttest: failed to unshare mount ns\n");
      exit(1);
    }

    // Cleanup to make sure we're clear
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0 ||
        make_sure_dir_doesnt_exist("/a") != 0) {
      return 1;
    }

    // Try pivotting from an invalid path to a valid path:
    // 1. Non existing path, should fail.
    if (pivot_root("/a", "/a/oldroot") == 0) {
      printf(stdout,
             "pivotrootmounttest: pivot root to invalid path succeeded\n");
      exit(1);
    }

    if (mkdir("/a") < 0 || mkdir("/a/oldroot") < 0) {
      printf(stdout, "pivotrootmounttest: failed to create old root dir\n");
      exit(1);
    }

    // 2. src exists but is not a mount point, should fail.
    if (pivot_root("/a", "/a/oldroot") == 0) {
      printf(stdout,
             "pivotrootmounttest: pivot root to invalid path succeeded\n");
      exit(1);
    }

    int res = mount("internal_fs_a", "/a", 0);
    if (res != 0) {
      printf(stdout, "pivotrootmounttest: mount returned %d\n", res);
      exit(1);
    }

    mkdir("/b");
    if (createfile(testfile, "in root mount") != 0) {
      printf(stdout, "pivotrootmounttest: failed to create test file\n");
      exit(1);
    }

    if (mount("/b", "/a/b", "bind") != 0) {
      printf(stdout, "pivotrootmounttest: failed to bind mount test mount\n");
      exit(1);
    }

    // Make sure /a/oldroot doesn't exist, and unlink it if it does
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0) {
      exit(1);
    }

    // Try pivot root from a valid path to an invalid paths:
    // 1.  (non existing), should fail.
    if (pivot_root("/a", "/a/oldroot") == 0) {
      printf(stdout,
             "pivotrootmounttest: pivot root to invalid path succeeded\n");
      exit(1);
    }
    // 2. put_old not under the new root, should fail.
    if (mkdir("/ab") < 0 || mkdir("/ab/oldroot") < 0) {
      printf(stdout, "pivotrootmounttest: failed to create invalid path\n");
      exit(1);
    }
    if (pivot_root("/a", "/ab/oldroot") == 0) {
      printf(stdout,
             "pivotrootmounttest: pivot root to invalid path succeeded\n");
      exit(1);
    }
    if (make_sure_dir_doesnt_exist("/ab/oldroot") != 0) {
      exit(1);
    }
    if (make_sure_dir_doesnt_exist("/ab") != 0) {
      exit(1);
    }
    // Now try to pivot correctly.
    if (mkdir("/a/oldroot") < 0) {
      printf(stdout, "pivotrootmounttest: failed to create old root dir\n");
      exit(1);
    }
    if (pivot_root("/a", "/a/oldroot") != 0) {
      printf(stdout, "pivotrootmounttest: failed to pivot root!\n");
      exit(1);
    }

    if (stat("/oldroot", &testfile_stat) != 0) {
      printf(stdout,
             "pivotrootmounttest: failed to find old root mount dir after "
             "pivot\n");
      exit(1);
    }

    if (chdir("/") < 0) {
      printf(stdout, "pivotrootmounttest: failed to chdir to root\n");
      exit(1);
    }

    // Remove old root mount
    if (umount("/oldroot") != 0) {
      printf(stdout, "pivotrootmounttest: failed to umount old root\n");
      exit(1);
    }

    if (stat(testfile, &testfile_stat) != 0) {
      printf(
          stdout,
          "pivotrootmounttest: failed to find test file in bind mounted dir\n");
      exit(1);
    }

    exit(0);
  } else {
    int test_status = -1;
    wait(&test_status);
    if (WEXITSTATUS(test_status) != 0) {
      return 1;
    }
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0) {
      return 1;
    }
    if (make_sure_dir_doesnt_exist("/a") != 0) {
      return 1;
    }

    return 0;
  }
}

static int pivotstresstest(void) {
  int pid = fork();
  if (pid == 0) {
    if (unshare(MOUNT_NS) != 0) {
      printf(stdout, "pivotstresstest: failed to unshare mount ns\n");
      exit(1);
    }
    // Cleanup to make sure we're clear
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0 ||
        make_sure_dir_doesnt_exist("/a") != 0) {
      return 1;
    }
    if (mkdir("/a") < 0 || mkdir("/a/oldroot") < 0) {
      printf(stdout, "pivotstresstest: failed to create old root dir\n");
      exit(1);
    }

    int res = mount("internal_fs_a", "/a", 0);
    if (res != 0) {
      printf(stdout, "pivotstresstest: mount returned %d\n", res);
      exit(1);
    }

    // Let's pivot_root 10 times -- pivot_root(/a, /a/oldroot) + chdir(/)
    // and then pivot_root(/oldroot, /oldroot/a) + chdir(/)
    for (int i = 0; i < 10; i++) {
      if (pivot_root("/a", "/a/oldroot") != 0) {
        printf(stdout, "pivotstresstest: failed to pivot root!\n");
        exit(1);
      }

      if (chdir("/") < 0) {
        printf(stdout, "pivotstresstest: failed to chdir to root\n");
        exit(1);
      }

      if (pivot_root("/oldroot", "/oldroot/a") != 0) {
        printf(stdout, "pivotstresstest: failed to pivot root back!\n");
        exit(1);
      }

      if (chdir("/") < 0) {
        printf(stdout, "pivotstresstest: failed to chdir to root\n");
        exit(1);
      }
    }
    if (umount("/a") != 0) {
      printf(stdout, "pivotstresstest: failed to umount old root\n");
      exit(1);
    }

    exit(0);
  } else {
    int test_status = -1;
    wait(&test_status);
    if (WEXITSTATUS(test_status) != 0) {
      return 1;
    }
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0) {
      return 1;
    }
    if (make_sure_dir_doesnt_exist("/a") != 0) {
      return 1;
    }

    return 0;
  }
}

static int pivot_and_umount_on_refd_root1() {
  // This test executes a pivot_root, and tries to umount the old root, but
  // should fail because the old root is still referenced in the new root.
  int pid = fork();
  if (pid == 0) {
    if (unshare(MOUNT_NS) != 0) {
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to unshare mount ns\n");
      exit(1);
    }
    // Cleanup to make sure we're clear
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0 ||
        make_sure_dir_doesnt_exist("/a") != 0) {
      return 1;
    }
    if (mkdir("/a") < 0 || mkdir("/a/oldroot") < 0) {
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to create old root dir\n");
      exit(1);
    }

    int res = mount("internal_fs_a", "/a", 0);
    if (res != 0) {
      printf(stdout, "pivot_and_umount_on_refd_root: mount returned %d\n", res);
      exit(1);
    }

    if (pivot_root("/a", "/a/oldroot") != 0) {
      printf(stdout, "pivot_and_umount_on_refd_root: failed to pivot root!\n");
      exit(1);
    }

    // our process still holds a reference to the old root as the current
    // working directory mount!
    if (umount("/oldroot") == 0) {
      printf(stdout,
             "pivot_and_umount_on_refd_root: umount succeeded on refd root\n");
      exit(1);
    }
    if (chdir("/") < 0) {  // deref
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to chdir to root\n");
      exit(1);
    }
    if (umount("/oldroot") != 0) {  // now it should work.
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to umount old root\n");
      exit(1);
    }

    exit(0);
  } else {
    int test_status = -1;
    wait(&test_status);
    if (WEXITSTATUS(test_status) != 0) {
      return 1;
    }
    return 0;
  }
}

static int pivot_and_umount_on_refd_root2() {
  // This is the same as _1, just that now, we'll be holding a reference to the
  // old root as we mount a filesyetm before pivot_root-ing on the old root, and
  // make sure we can only umount after we umount the new FS,
  int pid = fork();
  if (pid == 0) {
    if (unshare(MOUNT_NS) != 0) {
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to unshare mount ns\n");
      exit(1);
    }
    // Cleanup to make sure we're clear
    if (make_sure_dir_doesnt_exist("/a/oldroot") != 0 ||
        make_sure_dir_doesnt_exist("/a") != 0) {
      return 1;
    }
    if (mkdir("/a") < 0 || mkdir("/a/oldroot") < 0) {
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to create old root dir\n");
      exit(1);
    }

    int res = mount("internal_fs_a", "/a", 0);
    if (res != 0) {
      printf(stdout, "pivot_and_umount_on_refd_root: mount returned %d\n", res);
      exit(1);
    }

    // make sure we have a clean /rootref in the OLD root (we didn't pivot just
    // yet!)
    if (make_sure_dir_doesnt_exist("/rootref") != 0 || mkdir("/rootref") < 0) {
      return 1;
    }
    // mount objfs to /rootref in the old root
    if (mount(0, "/rootref", "objfs") != 0) {
      printf(
          stdout,
          "pivot_and_umount_on_refd_root: failed to mount objfs to /rootref\n");
      exit(1);
    }

    if (pivot_root("/a", "/a/oldroot") != 0) {
      printf(stdout, "pivot_and_umount_on_refd_root: failed to pivot root!\n");
      exit(1);
    }

    // our process still holds a reference to the old root as the current
    // working directory mount!
    if (umount("/oldroot") == 0) {
      printf(
          stdout,
          "pivot_and_umount_on_refd_root: umount succeeded on refd root2/2\n");
      exit(1);
    }
    if (chdir("/") < 0) {  // deref
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to chdir to root\n");
      exit(1);
    }
    if (umount("/oldroot") == 0) {  // still held by the objfs
      printf(
          stdout,
          "pivot_and_umount_on_refd_root: umount succeeded on refd root1/2\n");
      exit(1);
    }
    if (umount("/oldroot/rootref") != 0) {  // remove objfs ref.
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to umount objfs from old "
             "root\n");
      exit(1);
    }
    if (umount("/oldroot") != 0) {  // now it should work.
      printf(stdout,
             "pivot_and_umount_on_refd_root: failed to umount old root\n");
      exit(1);
    }

    exit(0);
  } else {
    int test_status = -1;
    wait(&test_status);
    if (WEXITSTATUS(test_status) != 0) {
      return 1;
    }
    return 0;
  }
}

static int make_sure_exited_cleanly(void) {
  if (make_sure_dir_doesnt_exist("/a/oldroot") != 0 ||
      make_sure_dir_doesnt_exist("/a") != 0) {
    return 1;
  }
  if (make_sure_dir_doesnt_exist("/rootref") != 0) {
    return 1;
  }
  if (verifylines("/proc/mounts", 1) != 0) {
    printf(stdout, "umountnonrootmount: expected a single mount\n");
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  printf(stderr, "Running all mounttest\n");
  run_test(mounttest, "mounttest");
  run_test(procmountstest, "procmounttest");
  run_test(procdevicestest, "procdevicestest");
  run_test(statroottest, "statroottest");
  run_test(invalidpathtest, "invalidpathtest");
  run_test(doublemounttest, "doublemounttest");
  run_test(samedirectorytest, "samedirectorytest");
  run_test(writefiletest, "writefiletest");
  run_test(directorywithintest, "directorywithintest");
  run_test(nestedmounttest, "nestedmounttest");
  run_test(devicefilestoretest, "devicefilestoretest");
  run_test(umountwithopenfiletest, "umountwithopenfiletest");
  run_test(errorondeletedevicetest, "errorondeletedevicetest");
  run_test(umountnonrootmount, "umountnonrootmount");
  run_test(pivotrootfiletest, "pivotrootfiletest");
  run_test(pivotrootmounttest, "pivotrootmounttest");
  run_test(pivotstresstest, "pivotstresstest");
  run_test(pivot_and_umount_on_refd_root1, "pivot_and_umount_on_refd_root1");
  run_test(pivot_and_umount_on_refd_root2, "pivot_and_umount_on_refd_root2");

  /* Tests that might leaves open mounts - leaves for last.
   * Other test might check how many open mounts there are
   *  and find unexpected value. */
  run_test(namespacetest, "namespacetest");
  run_test(namespacefiletest, "namespacefiletest");
  run_test(cdinthenouttest, "cdinthenouttest");

  unlink("a");
  unlink("b");

  run_test(make_sure_exited_cleanly, "make_sure_exited_cleanly");

  if (testsPassed == 0) {
    printf(stderr, "mounttest tests passed successfully\n");
    exit(0);
  } else {
    printf(stderr, "mounttest tests failed to pass\n");
    exit(1);
  }
}
