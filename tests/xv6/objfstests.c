#include "fcntl.h"
#include "fs.h"
#include "memlayout.h"
#include "param.h"
#include "stat.h"
#include "syscall.h"
#include "traps.h"
#include "types.h"
#include "user.h"
#include "wstatus.h"

char buf[8192];
char name[3];
char *echoargv[] = {"echo", "ALL", "OBJFSTESTS", "PASSED", 0};
int stdout = 1;

// another concurrent link/unlink/create test,
// to look for deadlocks.
void linkunlink() {
  int pid, i;

  printf(1, "linkunlink test\n");

  unlink("x");
  pid = fork();
  if (pid < 0) {
    printf(1, "fork failed\n");
    exit(1);
  }

  unsigned int x = (pid ? 1 : 97);
  for (i = 0; i < 100; i++) {
    x = x * 1103515245 + 12345;
    if ((x % 3) == 0) {
      close(open("x", O_RDWR | O_CREATE));
    } else if ((x % 3) == 1) {
      link("cat", "x");
    } else {
      unlink("x");
    }
  }

  if (pid)
    wait(0);
  else
    exit(0);

  printf(1, "linkunlink ok\n");
}

// four processes create and delete different files in same directory
void createdelete(void) {
  enum { N = 20 };
  int pid, i, fd, pi;
  char name[32];
  int process_count = 4;

  printf(1, "createdelete test\n");

  for (pi = 0; pi < process_count; pi++) {
    pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      name[0] = 'p' + pi;
      name[2] = '\0';
      for (i = 0; i < N; i++) {
        name[1] = '0' + i;
        fd = open(name, O_CREATE | O_RDWR);
        if (fd < 0) {
          printf(1, "create %s failed\n", name);
          exit(1);
        }
        close(fd);
        if (i > 0 && (i % 2) == 0) {
          name[1] = '0' + (i / 2);
          if (unlink(name) < 0) {
            printf(1, "unlink %s failed\n", name);
            exit(1);
          }
        }
      }
      exit(0);
    }
  }

  for (pi = 0; pi < process_count; pi++) {
    wait(0);
  }
  name[0] = name[1] = name[2] = 0;
  for (i = 0; i < N; i++) {
    for (pi = 0; pi < process_count; pi++) {
      name[0] = 'p' + pi;
      name[1] = '0' + i;
      fd = open(name, 0);
      if ((i == 0 || i >= N / 2) && fd < 0) {
        printf(1, "oops createdelete %s didn't exist\n", name);
        exit(1);
      } else if ((i >= 1 && i < N / 2) && fd >= 0) {
        printf(1, "oops createdelete %s did exist\n", name);
        exit(1);
      }
      if (fd >= 0) close(fd);
    }
  }

  for (i = 0; i < N; i++) {
    for (pi = 0; pi < process_count; pi++) {
      name[0] = 'p' + i;
      name[1] = '0' + i;
      unlink(name);
    }
  }

  printf(1, "createdelete ok\n");
}

// can I unlink a file and still read it?
void unlinkread(void) {
  int fd, fd1;

  printf(1, "unlinkread test\n");
  fd = open("unlinkread", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "create unlinkread failed\n");
    exit(1);
  }
  write(fd, "hello", 5);
  close(fd);

  fd = open("unlinkread", O_RDWR);
  if (fd < 0) {
    printf(1, "open unlinkread failed\n");
    exit(1);
  }
  if (unlink("unlinkread") != 0) {
    printf(1, "unlink unlinkread failed\n");
    exit(1);
  }

  fd1 = open("unlinkread", O_CREATE | O_RDWR);
  write(fd1, "yyy", 3);
  close(fd1);

  if (read(fd, buf, sizeof(buf)) != 5) {
    printf(1, "unlinkread read failed");
    exit(1);
  }
  if (buf[0] != 'h') {
    printf(1, "unlinkread wrong data\n");
    exit(1);
  }
  if (write(fd, buf, 10) != 10) {
    printf(1, "unlinkread write failed\n");
    exit(1);
  }
  close(fd);
  unlink("unlinkread");
  printf(1, "unlinkread ok\n");
}

// test concurrent create/link/unlink of the same file
#define FILES_COUNT (10)
void concreate(void) {
  char file[3];
  int i, pid, n, fd;
  char fa[FILES_COUNT];
  struct {
    ushort inum;
    char name[14];
  } de;

  printf(1, "concreate test\n");
  file[0] = 'C';
  file[2] = '\0';
  for (i = 0; i < FILES_COUNT; i++) {
    file[1] = '0' + i;
    unlink(file);
    pid = fork();
    if (pid && (i % 3) == 1) {
      link("C0", file);
    } else if (pid == 0 && (i % 5) == 1) {
      link("C0", file);
    } else {
      fd = open(file, O_CREATE | O_RDWR);
      if (fd < 0) {
        printf(1, "concreate create %s failed\n", file);
        exit(1);
      }
      close(fd);
    }
    if (pid == 0)
      exit(0);
    else
      wait(0);
  }

  memset(fa, 0, sizeof(fa));
  fd = open(".", 0);
  n = 0;
  while (read(fd, &de, sizeof(de)) > 0) {
    if (de.inum == 0) continue;
    if (de.name[0] == 'C' && de.name[2] == '\0') {
      i = de.name[1] - '0';
      if (i < 0 || i >= sizeof(fa)) {
        printf(1, "concreate weird file %s\n", de.name);
        exit(1);
      }
      if (fa[i]) {
        printf(1, "concreate duplicate file %s\n", de.name);
        exit(1);
      }
      fa[i] = 1;
      n++;
    }
  }
  close(fd);

  if (n != FILES_COUNT) {
    printf(1, "concreate not enough files in directory listing, n = %d\n", n);
    exit(1);
  }

  for (i = 0; i < FILES_COUNT; i++) {
    file[1] = '0' + i;
    pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit(1);
    }
    if (((i % 3) == 0 && pid == 0) || ((i % 3) == 1 && pid != 0)) {
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
      close(open(file, 0));
    } else {
      unlink(file);
      unlink(file);
      unlink(file);
      unlink(file);
    }
    if (pid == 0)
      exit(0);
    else
      wait(0);
  }

  printf(1, "concreate ok\n");
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void sharedfd(void) {
  int fd, pid, i, n, nc, np;
  char buf[10];

  printf(1, "sharedfd test\n");

  unlink("sharedfd");
  fd = open("sharedfd", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "fstests: cannot open sharedfd for writing");
    return;
  }
  pid = fork();
  memset(buf, pid == 0 ? 'c' : 'p', sizeof(buf));
  for (i = 0; i < 10; i++) {
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf(1, "fstests: write sharedfd failed\n");
      break;
    }
  }
  if (pid == 0)
    exit(0);
  else
    wait(0);
  close(fd);
  fd = open("sharedfd", 0);
  if (fd < 0) {
    printf(1, "fstests: cannot open sharedfd for reading\n");
    return;
  }
  nc = np = 0;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (i = 0; i < sizeof(buf); i++) {
      if (buf[i] == 'c') nc++;
      if (buf[i] == 'p') np++;
    }
  }
  close(fd);
  unlink("sharedfd");
  if (nc == 100 && np == 100) {
    printf(1, "sharedfd ok\n");
  } else {
    printf(1, "sharedfd oops %d %d\n", nc, np);
    exit(1);
  }
}

// four processes write different files at the same
// time, to test block allocation.
void fourfiles(void) {
  int fd, pid, i, j, n, total, pi;
  char *names[] = {"f0", "f1", "f2", "f3"};
  char *fname;

  printf(1, "fourfiles test\n");

  for (pi = 0; pi < 4; pi++) {
    fname = names[pi];
    unlink(fname);

    pid = fork();
    if (pid < 0) {
      printf(1, "fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      fd = open(fname, O_CREATE | O_RDWR);
      if (fd < 0) {
        printf(1, "create failed\n");
        exit(1);
      }

      memset(buf, '0' + pi, 512);
      for (i = 0; i < 12; i++) {
        if ((n = write(fd, buf, 20)) != 20) {
          printf(1, "write failed %d\n", n);
          exit(1);
        }
      }
      exit(0);
    }
  }

  for (pi = 0; pi < 4; pi++) {
    wait(0);
  }

  for (i = 0; i < 2; i++) {
    fname = names[i];
    fd = open(fname, 0);
    total = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
      for (j = 0; j < n; j++) {
        if (buf[j] != '0' + i) {
          printf(1, "wrong char\n");
          exit(1);
        }
      }
      total += n;
    }
    close(fd);
    if (total != 12 * 20) {
      printf(1, "wrong length %d\n", total);
      exit(1);
    }
    unlink(fname);
  }

  printf(1, "fourfiles ok\n");
}

// simple file system tests

void opentest(void) {
  int fd;

  printf(stdout, "open test\n");
  fd = open("some_file", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "create some_file failed!\n");
    exit(1);
  }
  close(fd);
  fd = open("some_file", 0);
  if (fd < 0) {
    printf(stdout, "open some_file failed!\n");
    exit(1);
  }
  close(fd);
  fd = open("doesnotexist", 0);
  if (fd >= 0) {
    printf(stdout, "open doesnotexist succeeded!\n");
    exit(1);
  }
  printf(stdout, "open test ok\n");
}

void writetest(void) {
  int fd;
  int i;

  printf(stdout, "small file test\n");
  fd = open("small", O_CREATE | O_RDWR);
  if (fd >= 0) {
    printf(stdout, "create small succeeded; ok\n");
  } else {
    printf(stdout, "error: create small failed!\n");
    exit(1);
  }
  for (i = 0; i < 50; i++) {
    if (write(fd, "aaaaaaaaaa", 10) != 10) {
      printf(stdout, "error: write aa %d new file failed\n", i);
      exit(1);
    }
    if (write(fd, "bbbbbbbbbb", 10) != 10) {
      printf(stdout, "error: write bb %d new file failed\n", i);
      exit(1);
    }
  }
  printf(stdout, "writes ok\n");
  close(fd);
  fd = open("small", O_RDONLY);
  if (fd >= 0) {
    printf(stdout, "open small succeeded ok\n");
  } else {
    printf(stdout, "error: open small failed!\n");
    exit(1);
  }
  i = read(fd, buf, 500);
  if (i == 500) {
    printf(stdout, "read succeeded ok\n");
  } else {
    printf(stdout, "read failed\n");
    exit(1);
  }
  close(fd);

  if (unlink("small") < 0) {
    printf(stdout, "unlink small failed\n");
    exit(1);
  }
  printf(stdout, "small file test ok\n");
}

void writetest1(void) {
  int i, fd, n, filesize;
  filesize = 1000;

  printf(stdout, "big files test\n");

  fd = open("big", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "error: create big failed!\n");
    exit(1);
  }

  for (i = 0; i < filesize; i++) {
    ((int *)buf)[0] = i;
    if (write(fd, buf, 1) != 1) {
      printf(stdout, "error: write big file failed\n", i);
      exit(1);
    }
  }

  close(fd);

  fd = open("big", O_RDONLY);
  if (fd < 0) {
    printf(stdout, "error: open big failed!\n");
    exit(1);
  }

  n = 0;
  for (;;) {
    i = read(fd, buf, 1);
    if (i == 0) {
      if (n == filesize - 1) {
        printf(stdout, "read only %d blocks from big", n);
        exit(0);
      }
      break;
    } else if (i != 1) {
      printf(stdout, "read failed %d\n", i);
      exit(1);
    }
    if (((int *)buf)[0] != n) {
      printf(stdout, "read content of block %d is %d\n", n, ((int *)buf)[0]);
      exit(0);
    }
    n++;
  }
  close(fd);
  if (unlink("big") < 0) {
    printf(stdout, "unlink big failed\n");
    exit(1);
  }
  printf(stdout, "big files ok\n");
}

void createtest(void) {
  int i, fd;

  printf(stdout, "many creates, followed by unlink test\n");

  name[0] = 'a';
  name[2] = '\0';
  for (i = 0; i < 10; i++) {
    name[1] = '0' + i;
    fd = open(name, O_CREATE | O_RDWR);
    close(fd);
  }
  name[0] = 'a';
  name[2] = '\0';
  for (i = 0; i < 10; i++) {
    name[1] = '0' + i;
    unlink(name);
  }
  printf(stdout, "many creates, followed by unlink; ok\n");
}

void dirtest(void) {
  printf(stdout, "mkdir test\n");

  if (mkdir("dir0") < 0) {
    printf(stdout, "mkdir failed\n");
    exit(1);
  }

  if (chdir("dir0") < 0) {
    printf(stdout, "chdir dir0 failed\n");
    exit(1);
  }

  if (chdir("..") < 0) {
    printf(stdout, "chdir .. failed\n");
    exit(1);
  }

  if (unlink("dir0") < 0) {
    printf(stdout, "unlink dir0 failed\n");
    exit(1);
  }
  printf(stdout, "mkdir test ok\n");
}

// does chdir() call iput(p->cwd) in a transaction?
void iputtest(void) {
  printf(stdout, "iput test\n");

  if (mkdir("iputdir") < 0) {
    printf(stdout, "mkdir failed\n");
    exit(1);
  }
  if (chdir("iputdir") < 0) {
    printf(stdout, "chdir iputdir failed\n");
    exit(1);
  }
  if (unlink("../iputdir") < 0) {
    printf(stdout, "unlink ../iputdir failed\n");
    exit(1);
  }
  if (chdir("/new") < 0) {
    printf(stdout, "chdir /new failed\n");
    exit(1);
  }
  printf(stdout, "iput test ok\n");
}

// simple fork and pipe read/write

void pipe1(void) {
  int fds[2], pid;
  int seq, i, n, cc, total;

  printf(stdout, "pipe1 test\n");

  if (pipe(fds) != 0) {
    printf(stdout, "pipe1 failed test\n");

    printf(1, "pipe() failed\n");
    exit(1);
  }
  pid = fork();
  seq = 0;
  printf(stdout, "pipe1 after fork test\n");

  if (pid == 0) {
    printf(stdout, "pipe1 pid == 0 test\n");

    close(fds[0]);
    for (n = 0; n < 5; n++) {
      for (i = 0; i < 1033; i++) buf[i] = seq++;
      if (write(fds[1], buf, 1033) != 1033) {
        printf(1, "pipe1 oops 1\n");
        exit(1);
      }
    }
    printf(1, "pipe1 oops 1.5\n");

    exit(0);
  } else if (pid > 0) {
    printf(stdout, "pipe1 pid > 0 test\n");

    close(fds[1]);
    total = 0;
    cc = 1;
    while ((n = read(fds[0], buf, cc)) > 0) {
      for (i = 0; i < n; i++) {
        if ((buf[i] & 0xff) != (seq++ & 0xff)) {
          printf(1, "pipe1 oops 2\n");
          return;
        }
      }
      total += n;
      cc = cc * 2;
      if (cc > sizeof(buf)) cc = sizeof(buf);
    }
    if (total != 5 * 1033) {
      printf(1, "pipe1 oops 3 total %d\n", total);
      exit(1);
    }
    close(fds[0]);
    wait(0);
  } else {
    printf(1, "fork() failed\n");
    exit(1);
  }
  printf(1, "pipe1 ok\n");
}

void rmdot(void) {
  printf(1, "rmdot test\n");
  if (mkdir("dots") != 0) {
    printf(1, "mkdir dots failed\n");
    exit(1);
  }
  if (chdir("dots") != 0) {
    printf(1, "chdir dots failed\n");
    exit(1);
  }
  if (unlink(".") == 0) {
    printf(1, "rm . worked!\n");
    exit(1);
  }
  if (unlink("..") == 0) {
    printf(1, "rm .. worked!\n");
    exit(1);
  }
  if (chdir("/new") != 0) {
    printf(1, "chdir /new failed\n");
    exit(1);
  }
  if (unlink("dots/.") == 0) {
    printf(1, "unlink dots/. worked!\n");
    exit(1);
  }
  if (unlink("dots/..") == 0) {
    printf(1, "unlink dots/.. worked!\n");
    exit(1);
  }
  if (unlink("dots") != 0) {
    printf(1, "unlink dots failed!\n");
    exit(1);
  }
  printf(1, "rmdot ok\n");
}

void fourteen(void) {
  int fd;

  // DIRSIZ is 14.
  printf(1, "fourteen test\n");

  if (mkdir("12345678901234") != 0) {
    printf(1, "mkdir 12345678901234 failed\n");
    exit(1);
  }
  if (mkdir("12345678901234/123456789012345") != 0) {
    printf(1, "mkdir 12345678901234/123456789012345 failed\n");
    exit(1);
  }
  fd = open("123456789012345/123456789012345/123456789012345", O_CREATE);
  if (fd < 0) {
    printf(1,
           "create 123456789012345/123456789012345/123456789012345 failed\n");
    exit(1);
  }
  close(fd);
  fd = open("12345678901234/12345678901234/12345678901234", 0);
  if (fd < 0) {
    printf(1, "open 12345678901234/12345678901234/12345678901234 failed\n");
    exit(1);
  }
  close(fd);

  if (mkdir("12345678901234/12345678901234") == 0) {
    printf(1, "mkdir 12345678901234/12345678901234 succeeded!\n");
    exit(1);
  }
  if (mkdir("123456789012345/12345678901234") == 0) {
    printf(1, "mkdir 12345678901234/123456789012345 succeeded!\n");
    exit(1);
  }

  printf(1, "fourteen ok\n");
}

void bigfile(void) {
  int fd, i, total, cc;
  int write_iterations = 20;
  int bytes_per_write = 100;

  printf(1, "bigfile test\n");

  unlink("bigfile");
  fd = open("bigfile", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "cannot create bigfile");
    exit(1);
  }
  // write
  for (i = 0; i < write_iterations; i++) {
    memset(buf, i, bytes_per_write);
    if (write(fd, buf, bytes_per_write) != bytes_per_write) {
      printf(1, "write bigfile failed\n");
      exit(1);
    }
  }
  close(fd);

  fd = open("bigfile", 0);
  if (fd < 0) {
    printf(1, "cannot open bigfile\n");
    exit(1);
  }
  total = 0;
  for (i = 0;; i++) {
    cc = read(fd, buf, bytes_per_write);
    if (cc < 0) {
      printf(1, "read bigfile failed\n");
      exit(1);
    }
    if (cc == 0) break;
    if (cc != bytes_per_write) {
      printf(1, "short read bigfile\n");
      exit(1);
    }
    if (buf[0] != i || buf[bytes_per_write - 1] != i) {
      printf(1, "read bigfile wrong data\n");
      exit(1);
    }
    total += cc;
  }
  close(fd);
  if (total != write_iterations * bytes_per_write) {
    printf(1, "read bigfile wrong total\n");
    exit(1);
  }
  unlink("bigfile");

  printf(1, "bigfile test ok\n");
}

void linktest(void) {
  int fd;

  printf(1, "linktest\n");

  unlink("lf1");
  unlink("lf2");

  fd = open("lf1", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "create lf1 failed\n");
    exit(1);
  }
  if (write(fd, "hello", 5) != 5) {
    printf(1, "write lf1 failed\n");
    exit(1);
  }
  close(fd);

  if (link("lf1", "lf2") < 0) {
    printf(1, "link lf1 lf2 failed\n");
    exit(1);
  }
  unlink("lf1");

  if (open("lf1", 0) >= 0) {
    printf(1, "unlinked lf1 but it is still there!\n");
    exit(1);
  }

  fd = open("lf2", 0);
  if (fd < 0) {
    printf(1, "open lf2 failed\n");
    exit(1);
  }
  if (read(fd, buf, sizeof(buf)) != 5) {
    printf(1, "read lf2 failed\n");
    exit(1);
  }
  close(fd);

  if (link("lf2", "lf2") >= 0) {
    printf(1, "link lf2 lf2 succeeded! oops\n");
    exit(1);
  }

  unlink("lf2");
  if (link("lf2", "lf1") >= 0) {
    printf(1, "link non-existant succeeded! oops\n");
    exit(1);
  }

  if (link(".", "lf1") >= 0) {
    printf(1, "link . lf1 succeeded! oops\n");
    exit(1);
  }

  printf(1, "linktest ok\n");
}

void subdir(void) {
  int fd, cc;

  printf(1, "subdir test\n");

  unlink("ff");
  if (mkdir("dd") != 0) {
    printf(1, "subdir mkdir dd failed\n");
    exit(1);
  }

  fd = open("dd/ff", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "create dd/ff failed\n");
    exit(1);
  }
  write(fd, "ff", 2);
  close(fd);

  if (unlink("dd") >= 0) {
    printf(1, "unlink dd (non-empty dir) succeeded!\n");
    exit(1);
  }

  if (mkdir("/new/dd/dd") != 0) {
    printf(1, "subdir mkdir dd/dd failed\n");
    exit(1);
  }

  fd = open("dd/dd/ff", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "create dd/dd/ff failed\n");
    exit(1);
  }
  write(fd, "FF", 2);
  close(fd);

  fd = open("dd/dd/../ff", 0);
  if (fd < 0) {
    printf(1, "open dd/dd/../ff failed\n");
    exit(1);
  }
  cc = read(fd, buf, sizeof(buf));
  if (cc != 2 || buf[0] != 'f') {
    printf(1, "dd/dd/../ff wrong content\n");
    exit(1);
  }
  close(fd);

  if (link("dd/dd/ff", "dd/dd/ffff") != 0) {
    printf(1, "link dd/dd/ff dd/dd/ffff failed\n");
    exit(1);
  }

  if (unlink("dd/dd/ff") != 0) {
    printf(1, "unlink dd/dd/ff failed\n");
    exit(1);
  }
  if (open("dd/dd/ff", O_RDONLY) >= 0) {
    printf(1, "open (unlinked) dd/dd/ff succeeded\n");
    exit(1);
  }

  if (chdir("dd") != 0) {
    printf(1, "chdir dd failed\n");
    exit(1);
  }
  if (chdir("dd/../../dd") != 0) {
    printf(1, "chdir dd/../../dd failed\n");
    exit(1);
  }
  if (chdir("./..") != 0) {
    printf(1, "chdir ./.. failed\n");
    exit(1);
  }

  fd = open("dd/dd/ffff", 0);
  if (fd < 0) {
    printf(1, "open dd/dd/ffff failed\n");
    exit(1);
  }
  if (read(fd, buf, sizeof(buf)) != 2) {
    printf(1, "read dd/dd/ffff wrong len\n");
    exit(1);
  }
  close(fd);

  if (open("dd/dd/ff", O_RDONLY) >= 0) {
    printf(1, "open (unlinked) dd/dd/ff succeeded!\n");
    exit(1);
  }

  if (open("dd/ff/ff", O_CREATE | O_RDWR) >= 0) {
    printf(1, "create dd/ff/ff succeeded!\n");
    exit(1);
  }
  if (open("dd/xx/ff", O_CREATE | O_RDWR) >= 0) {
    printf(1, "create dd/xx/ff succeeded!\n");
    exit(1);
  }
  if (open("dd", O_CREATE) >= 0) {
    printf(1, "create dd succeeded!\n");
    exit(1);
  }
  if (open("dd", O_RDWR) >= 0) {
    printf(1, "open dd rdwr succeeded!\n");
    exit(1);
  }
  if (open("dd", O_WRONLY) >= 0) {
    printf(1, "open dd wronly succeeded!\n");
    exit(1);
  }
  if (link("dd/ff/ff", "dd/dd/xx") == 0) {
    printf(1, "link dd/ff/ff dd/dd/xx succeeded!\n");
    exit(1);
  }
  if (link("dd/xx/ff", "dd/dd/xx") == 0) {
    printf(1, "link dd/xx/ff dd/dd/xx succeeded!\n");
    exit(1);
  }
  if (link("dd/ff", "dd/dd/ffff") == 0) {
    printf(1, "link dd/ff dd/dd/ffff succeeded!\n");
    exit(1);
  }
  if (mkdir("dd/ff/ff") == 0) {
    printf(1, "mkdir dd/ff/ff succeeded!\n");
    exit(1);
  }
  if (mkdir("dd/xx/ff") == 0) {
    printf(1, "mkdir dd/xx/ff succeeded!\n");
    exit(1);
  }
  if (mkdir("dd/dd/ffff") == 0) {
    printf(1, "mkdir dd/dd/ffff succeeded!\n");
    exit(1);
  }
  if (unlink("dd/xx/ff") == 0) {
    printf(1, "unlink dd/xx/ff succeeded!\n");
    exit(1);
  }
  if (unlink("dd/ff/ff") == 0) {
    printf(1, "unlink dd/ff/ff succeeded!\n");
    exit(1);
  }
  if (chdir("dd/ff") == 0) {
    printf(1, "chdir dd/ff succeeded!\n");
    exit(1);
  }
  if (chdir("dd/xx") == 0) {
    printf(1, "chdir dd/xx succeeded!\n");
    exit(1);
  }

  if (unlink("dd/dd/ffff") != 0) {
    printf(1, "unlink dd/dd/ff failed\n");
    exit(1);
  }
  if (unlink("dd/ff") != 0) {
    printf(1, "unlink dd/ff failed\n");
    exit(1);
  }
  if (unlink("dd") == 0) {
    printf(1, "unlink non-empty dd succeeded!\n");
    exit(1);
  }
  if (unlink("dd/dd") < 0) {
    printf(1, "unlink dd/dd failed\n");
    exit(1);
  }
  if (unlink("dd") < 0) {
    printf(1, "unlink dd failed\n");
    exit(1);
  }

  printf(1, "subdir ok\n");
}

void dirfile(void) {
  int fd;

  printf(1, "dir vs file\n");

  fd = open("dirfile", O_CREATE);
  if (fd < 0) {
    printf(1, "create dirfile failed\n");
    exit(1);
  }
  close(fd);
  if (chdir("dirfile") == 0) {
    printf(1, "chdir dirfile succeeded!\n");
    exit(1);
  }
  fd = open("dirfile/xx", 0);
  if (fd >= 0) {
    printf(1, "create dirfile/xx succeeded!\n");
    exit(1);
  }
  fd = open("dirfile/xx", O_CREATE);
  if (fd >= 0) {
    printf(1, "create dirfile/xx succeeded!\n");
    exit(1);
  }
  if (mkdir("dirfile/xx") == 0) {
    printf(1, "mkdir dirfile/xx succeeded!\n");
    exit(1);
  }
  if (unlink("dirfile/xx") == 0) {
    printf(1, "unlink dirfile/xx succeeded!\n");
    exit(1);
  }
  if (link("README", "dirfile/xx") == 0) {
    printf(1, "link to dirfile/xx succeeded!\n");
    exit(1);
  }
  if (unlink("dirfile") != 0) {
    printf(1, "unlink dirfile failed!\n");
    exit(1);
  }

  fd = open(".", O_RDWR);
  if (fd >= 0) {
    printf(1, "open . for writing succeeded!\n");
    exit(1);
  }
  fd = open(".", 0);
  if (write(fd, "x", 1) > 0) {
    printf(1, "write . succeeded!\n");
    exit(1);
  }
  close(fd);

  printf(1, "dir vs file OK\n");
}

void exectest(void) {
  printf(stdout, "exec test\n");
  if (exec("/echo", echoargv) < 0) {
    printf(stdout, "exec echo failed\n");
    exit(1);
  }
}

// test that iput() is called at the end of _namei()
void iref(void) {
  int i, fd;

  printf(1, "empty file name\n");

  for (i = 0; i < 20; i++) {
    if (mkdir("irefd") != 0) {
      printf(1, "mkdir irefd failed\n");
      exit(1);
    }
    if (chdir("irefd") != 0) {
      printf(1, "chdir irefd failed\n");
      exit(1);
    }

    mkdir("");
    link("README", "");
    fd = open("", O_CREATE);
    if (fd >= 0) close(fd);
    fd = open("xx", O_CREATE);
    if (fd >= 0) close(fd);
    unlink("xx");
  }

  chdir("/new");
  printf(1, "empty file name OK\n");
}

void createmanyfiles(uint number_of_files_to_create) {
  printf(stdout, "create many files\n");
  char filename[100] = "file";
  for (int i = 0; i < number_of_files_to_create; i++) {
    // generate filename
    itoa(filename + 4, i);
    int fd = open(filename, O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(1, "create %s failed\n", filename);
      exit(1);
    }
    close(fd);
  }
  printf(stdout, "create many files ok\n");
}

/** obj cache tests
 * these tests tests the functionallity of the object cache
 */

/**
 * helper functions
 */

/**
 * get a string that represents a uint and return the int.
 *
 * @return the int if successful and -1 if there is an error
 */
int string_to_int(char *s) {
  int result = 0;

  // Iterate over the string
  for (int i = 0; s[i] != '\0'; i++) {
    // Update the result by multiplying it by 10 and adding the righmost
    // digit
    result *= 10;
    result += s[i] - '0';
  }

  return result;
}

/**
 * Read a file that contains a uint property of the object cache
 *
 * @param path_to_file path to the file in /proc that contains the requested
 * property
 * @return the property if successful, -1 if not
 */
int object_cache_property_from_procfs(char *path_to_file) {
  int fd, cc;
  char size_buffer[20];
  // Get cache block size from procfs
  fd = open(path_to_file, O_RDONLY);
  // I asseme here that the property is less that 20 digits
  cc = read(fd, size_buffer, 20);

  // If the read was not successful, return -1
  if (cc < 0) {
    return -1;
  }

  size_buffer[cc] = '\0';

  return string_to_int(size_buffer);
}

/**
 * get the object cache's block size from procfs
 *
 * @return the size if successful, -1 if not
 */
int object_cache_block_size() {
  return object_cache_property_from_procfs("/proc/obj_cache_block_size");
}

/**
 * get the object cache's number of blocks from procfs
 *
 * @return the number of blocks if successful, -1 if not
 */
int object_cache_number_of_blocks() {
  return object_cache_property_from_procfs("/proc/obj_cache_blocks");
}

/**
 * get the object cache's number of blocks per object from procfs
 *
 * @return the number of blocks per object if successful, -1 if not
 */
int object_cache_max_blocks_per_object() {
  return object_cache_property_from_procfs("/proc/obj_cache_blocks_per_object");
}

/**
 * get the object cache's number of cache hits procfs
 *
 * @return the number of hits if successful, -1 if not
 */
int object_cache_hits() {
  return object_cache_property_from_procfs("/proc/obj_cache_hits");
}

/**
 * get the object cache's number of cache misses procfs
 *
 * @return the number of misses if successful, -1 if not
 */
int object_cache_misses() {
  return object_cache_property_from_procfs("/proc/obj_cache_misses");
}

/**
 * this helper function gets a number and returns a file name unique to that
 * number
 */
void generate_file_name_for_number(int n, char *buffer) {
  char base_file_name[] = "file     ";
  int digits[5];
  for (int i = 4; i >= 0; i--) {
    digits[i] = n % 10;
    n /= 10;
  }
  int i;
  for (i = 4; i < 9; i++) {
    base_file_name[i] = '0' + digits[i - 4];
  }
  base_file_name[i] = '\0';
  strcpy(buffer, base_file_name);
}

/**
 * obj cache tests
 */

/**
 * test that the procfs files return the values we expect them to return
 */
void objprocfs() {
  printf(1, "objfrocfs test starting\n");
  // expected values
  int expected_block_size = 1024;
  int expected_number_of_blocks = 100;
  int expected_max_blocks_per_object = 8;

  // actual values
  int block_size_from_procfs = object_cache_block_size();
  int number_of_blocks_from_procfs = object_cache_number_of_blocks();
  int max_blocks_per_object_from_procfs = object_cache_max_blocks_per_object();

  if (expected_block_size != block_size_from_procfs) {
    printf(1, "expected block size: %d, actual: %d, exiting\n",
           expected_block_size, block_size_from_procfs);
    exit(1);
  }

  if (expected_number_of_blocks != number_of_blocks_from_procfs) {
    printf(1, "expected number of blocks: %d, actual: %d, exiting\n",
           expected_number_of_blocks, number_of_blocks_from_procfs);
    exit(1);
  }

  if (expected_max_blocks_per_object != max_blocks_per_object_from_procfs) {
    printf(1, "expected max blocks per object: %d, actual: %d, exiting\n",
           expected_max_blocks_per_object, max_blocks_per_object_from_procfs);
    exit(1);
  }
  printf(1, "objfrocfs test finished successfully\n");
}

/**
 * In this test we check that the hits and misses go up when we expect them to.
 * We can't use hits and misses more in the tests because we are running in
 * userspace, so when we write an object things that affect those statistics
 * happen in the background- like the log layer writes.
 */
void teststats() {
  printf(1, "teststats starting\n");
  int prev_hits = object_cache_hits();
  int prev_misses = object_cache_misses();
  int fd, cc, curr_hits, curr_misses;
  char filename[] = "stattest";
  // write a new file- we expect the misses to go up and the hits to not go down
  fd = open(filename, O_CREATE | O_RDWR);
  cc = write(fd, "hello", strlen("hello"));
  if (cc < 0) {
    printf(1, "write failed\n");
    exit(1);
  }
  curr_hits = object_cache_hits();
  curr_misses = object_cache_misses();

  // we expect the hits to at least remain the same and the misses to go up
  // because we accessed a new file
  if (curr_hits < prev_hits || curr_misses <= prev_misses) {
    printf(1,
           "unexpected hits and misses after writing. prev_hits: %d curr hits: "
           "%d, prev misses: %d curr misses: %d ",
           prev_hits, curr_hits, prev_misses, curr_misses);
    exit(1);
  }
  prev_hits = curr_hits;
  prev_misses = curr_misses;
  close(fd);
  fd = open(filename, O_RDONLY);
  read(fd, buf, strlen("hello") + 1);

  curr_hits = object_cache_hits();
  curr_misses = object_cache_misses();

  // we expect the misses to at least remain the same and the hits to go up
  // because we accessed an existing file
  if (curr_hits < prev_hits || curr_misses <= prev_misses) {
    printf(1,
           "unexpected hits and misses after reading. prev_hits: %d curr hits: "
           "%d, prev misses: %d curr misses: %d ",
           prev_hits, curr_hits, prev_misses, curr_misses);
    exit(1);
  }
  close(fd);
  unlink(filename);
  printf(1, "teststats finished successfully\n");
}

/**
 * This test tests that we can write an object that is bigger than a single
 * cache block but is small enough to be cached in one write
 */
void biggerthancacheblocksinglewrite(void) {
  int fd, cc;
  printf(1, "biggerthancacheblocksinglewrite test\n");

  int block_size = object_cache_block_size();
  if (block_size < 0) {
    printf(1, "failed to get object cache block size, exiting\n");
    exit(1);
  }

  int bytes_to_write = block_size * 1.5;

  unlink("big_file_single");
  fd = open("big_file_single", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "cannot create big_file_single\n");
    exit(1);
  }
  // write
  memset(buf, 60, bytes_to_write);
  uint written = write(fd, buf, bytes_to_write);
  if (written != bytes_to_write) {
    printf(1, "write big_file_single failed\nWrote %d out of %d\n", written,
           bytes_to_write);
    exit(1);
  }
  close(fd);

  fd = open("big_file_single", 0);
  if (fd < 0) {
    printf(1, "cannot open big_file_single\n");
    exit(1);
  }
  cc = read(fd, buf, bytes_to_write);
  if (cc < 0) {
    printf(1, "read big_file_single failed\n, err_code: %d\n", cc);
    exit(1);
  }
  if (cc != bytes_to_write) {
    printf(1, "read big_file_single failed\n");
    exit(1);
  }

  close(fd);
  unlink("big_file_single");

  printf(1, "biggerthancacheblocksinglewrite test ok\n");
}

/**
 * This test tests that we can write an object that is the size of 10 cache
 * blocks in 100 small rewrites, which means that the object will be in the
 * cache at the start and will not be in the cache at the end
 */
void biggerthanmaxcachesize(void) {
  int fd, i, total, cc;
  int write_iterations = 100;

  printf(1, "biggerthancachesize test\n");

  int block_size = object_cache_block_size();
  if (block_size < 0) {
    printf(1, "failed to get object cache block size, exiting");
    exit(1);
  }
  int bytes_per_write = block_size / 10;

  unlink("bigger_than_block");
  fd = open("bigger_than_block", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "cannot create bigger_than_block");
    exit(1);
  }
  // write
  for (i = 0; i < write_iterations; i++) {
    memset(buf, i, bytes_per_write);
    uint written = write(fd, buf, bytes_per_write);
    if (written != bytes_per_write) {
      printf(1,
             "write bigger_than_block failed\nWrote %d out of %d\nIteration: "
             "%d/%d",
             written, bytes_per_write, i, write_iterations);
      exit(1);
    }
  }
  close(fd);

  fd = open("bigger_than_block", 0);
  if (fd < 0) {
    printf(1, "cannot open bigger_than_block\n");
    exit(1);
  }
  total = 0;
  for (i = 0;; i++) {
    cc = read(fd, buf, bytes_per_write);
    if (cc < 0) {
      printf(1, "read bigger_than_block failed\n, err_code: %d", cc);
      exit(1);
    }
    if (cc == 0) break;
    if (cc != bytes_per_write) {
      printf(1, "short read bigger_than_block\n");
      exit(1);
    }
    if (buf[0] != i || buf[bytes_per_write - 1] != i) {
      printf(1, "read bigger_than_block wrong data\n");
      exit(1);
    }
    total += cc;
  }
  close(fd);
  if (total != write_iterations * bytes_per_write) {
    printf(1, "read bigger_than_block wrong total\n");
    exit(1);
  }
  unlink("bigger_than_block");

  printf(1, "biggerthancachesize test ok\n");
}

/**
 * In this tests we write an read a lot of objects to the cache and check that
 * we get the expected outputs. The working set in this test is bigger than the
 * number of blocks in the cache so it tests the case of thrashing.
 */
void objcachestressbigworkingset() {
  printf(1, "objcachestressbigworkingset starting\n");
  int fd, cc;
  int cache_entries = object_cache_number_of_blocks();
  for (int i = 0; i < 2 * cache_entries; i++) {
    generate_file_name_for_number(i, buf);

    fd = open(buf, O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(1, "could not open file %s\n", buf);
      exit(1);
    }
    cc = write(fd, buf, strlen(buf) + 1);
    if (cc != strlen(buf) + 1) {
      printf(1, "could not write file %s", buf);
    }
    close(fd);
    // uncomment for debugging
    // printf(1, "done writing file %s\n", buf);
  }

  for (int i = 0; i < 2 * cache_entries; i++) {
    generate_file_name_for_number(i, buf);

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
      printf(1,
             "objcachestressbigworkingset: could not open file for reading%s\n",
             buf);
      exit(1);
    }
    char *buf2 = buf + 1000;
    cc = read(fd, buf2, strlen(buf) + 1);
    if (cc != strlen(buf) + 1) {
      printf(1, "could not read file %s", buf);
    }
    if (strcmp(buf, buf2) != 0) {
      printf(1,
             "objcachestressbigworkingset: contents of file %s are not as "
             "expected: the expected is %s but "
             "we got %s\n",
             buf, buf, buf2);
      exit(1);
    }
    close(fd);
    unlink(buf);
    // uncomment for debugging
    // printf(1, "done reading file %s\n", buf);
  }
  printf(1, "objcachestressbigworkingset finished successfully\n");
}

/**
 * In this tests we write an read a lot of objects to the cache and check that
 * we get the expected outputs. The working set in this test is smaller than the
 * number of blocks in the cache so it tests the case of retrieving many objects
 * from the cache.
 */
void objcachestresssmallworkingset() {
  printf(1, "objcachestresssmallworkingset starting\n");
  int fd, cc;
  int cache_entries = object_cache_number_of_blocks();
  for (int i = 0; i < cache_entries / 2; i++) {
    generate_file_name_for_number(i, buf);

    fd = open(buf, O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(1, "objcachestresssmallworkingset: could not open file %s\n", buf);
      exit(1);
    }
    cc = write(fd, buf, strlen(buf) + 1);
    if (cc != strlen(buf) + 1) {
      printf(1, "objcachestresssmallworkingset: could not write file %s", buf);
    }
    close(fd);
    // uncomment for debugging
    // printf(1, "done writing file %s\n", buf);
  }

  for (int i = 0; i < cache_entries / 2; i++) {
    generate_file_name_for_number(i, buf);

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
      printf(
          1,
          "objcachestresssmallworkingset: could not open file for reading%s\n",
          buf);
      exit(1);
    }
    char *buf2 = buf + 1000;
    cc = read(fd, buf2, strlen(buf) + 1);
    if (cc != strlen(buf) + 1) {
      printf(1, "objcachestresssmallworkingset: could not read file %s", buf);
    }
    if (strcmp(buf, buf2) != 0) {
      printf(1,
             "objcachestresssmallworkingset: contents of file %s are not as "
             "expected: the expected is %s but "
             "we got %s\n",
             buf, buf, buf2);
      exit(1);
    }
    close(fd);
    unlink(buf);
    // uncomment for debugging
    // printf(1, "done reading file %s\n", buf);
  }
  printf(1, "objcachestresssmallworkingset finished successfully\n");
}

/**
 * Main
 */
int main(int argc, char *argv[]) {
  printf(1, "objfstests starting\n");

  if (mkdir("new") != 0) {
    printf(2, "objtest: failed to create dir new\n");
  }

  if (mount(0, "new", "objfs") != 0) {
    printf(2, "objtest: failed to mount objfs to new\n");
    exit(0);
  }

  if (chdir("/new") < 0) {
    printf(2, "ls: cannot cd new\n");
    exit(0);
  }

  // obj cache tests
  objprocfs();
  teststats();
  biggerthancacheblocksinglewrite();
  biggerthanmaxcachesize();
  objcachestressbigworkingset();
  objcachestresssmallworkingset();

  createdelete();
  createmanyfiles(300);
  linkunlink();
  concreate();
  fourfiles();
  sharedfd();
  opentest();
  writetest();
  writetest1();
  createtest();
  iputtest();
  pipe1();
  rmdot();
  fourteen();
  bigfile();
  subdir();
  linktest();
  unlinkread();
  dirfile();
  iref();

  exectest();  // Ensure this test to be the last one to run (prints ALL TESTS
               // PASSED)

  exit(0);
}
