#include "fcntl.h"
#include "fsdefs.h"
#include "kernel/memlayout.h"
#include "param.h"
#include "stat.h"
#include "syscall.h"
#include "traps.h"
#include "types.h"
#include "user/lib/user.h"
#include "wstatus.h"

char buf[8192];
char name[3];
char *echoargv[] = {"/echo", "ALL", "TESTS", "PASSED", 0};

// does chdir() call iput(p->cwd) in a transaction?
void iputtest(const char *fs_type) {
  printf(stdout, "%s iput test\n", fs_type);

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
  if (chdir("..") < 0) {
    printf(stdout, "chdir / failed\n");
    exit(1);
  }
  printf(stdout, "%s iput test ok\n", fs_type);
}

// does exit() call iput(p->cwd) in a transaction?
void exitiputtest(const char *fs_type) {
  int pid;

  printf(stdout, "%s exitiput test\n", fs_type);

  pid = fork();
  if (pid < 0) {
    printf(stdout, "fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    if (mkdir("iputdir") < 0) {
      printf(stdout, "mkdir failed\n");
      exit(1);
    }
    if (chdir("iputdir") < 0) {
      printf(stdout, "child chdir failed\n");
      exit(1);
    }
    if (unlink("../iputdir") < 0) {
      printf(stdout, "unlink ../iputdir failed\n");
      exit(1);
    }
    exit(0);
  }
  wait(0);
  printf(stdout, "%s exitiput test ok\n", fs_type);
}

// does the error path in open() for attempt to write a
// directory call iput() in a transaction?
// needs a hacked kernel that pauses just after the namei()
// call in sys_open():
//    if((ip = namei(path)) == 0)
//      return -1;
//    {
//      int i;
//      for(i = 0; i < 10000; i++)
//        yield();
//    }
void openiputtest(const char *fs_type) {
  int pid;

  printf(stdout, "%s openiput test\n", fs_type);
  if (mkdir("oidir") < 0) {
    printf(stdout, "mkdir oidir failed\n");
    exit(1);
  }
  pid = fork();
  if (pid < 0) {
    printf(stdout, "fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    int fd = open("oidir", O_RDWR);
    if (fd >= 0) {
      printf(stdout, "open directory for write succeeded\n");
      exit(0);
    }
    exit(0);
  }
  sleep(1);
  if (unlink("oidir") != 0) {
    printf(stdout, "unlink failed\n");
    exit(1);
  }
  wait(0);
  printf(stdout, "%s openiput test ok\n", fs_type);
}

// simple file system tests

void opentest(const char *fs_type) {
  int fd;

  printf(stdout, "%s open test\n", fs_type);
  fd = open("/echo", 0);
  if (fd < 0) {
    printf(stdout, "open /echo failed!\n");
    exit(1);
  }

  close(fd);
  fd = open("doesnotexist", 0);
  if (fd >= 0) {
    printf(stdout, "open doesnotexist succeeded!\n");
    exit(1);
  }

  for (int i = 0; i < 2; i++) {
    fd = open("createthis", O_CREATE);
    if (fd < 0) {
      printf(stdout, "open create failed!\n");
      exit(1);
    }
    close(fd);
  }

  printf(stdout, "%s open test ok\n", fs_type);
}

void openexclusivetest(const char *fs_type) {
  int fd;

  printf(stdout, "%s open exclusive test\n", fs_type);
  for (int i = 0; i < 2; i++) {
    fd = open("createthisexcl", O_CREATE | O_EXCL);
    if ((i == 0 && fd < 0) || (i > 0 && fd >= 0)) {
      printf(stdout, "open O_CREATE | O_EXCL iteration #%d error\n", i);
      exit(1);
    }
    close(fd);
  }

  printf(stdout, "%s open exclusive test ok\n", fs_type);
}

void writetest(const char *fs_type) {
  int fd;
  int i;

  printf(stdout, "%s small file test\n", fs_type);
  fd = open("small", O_CREATE | O_RDWR);
  if (fd >= 0) {
    printf(stdout, "creat small succeeded; ok\n");
  } else {
    printf(stdout, "error: creat small failed!\n");
    exit(1);
  }
  for (i = 0; i < 100; i++) {
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
  i = read(fd, buf, 2000);
  if (i == 2000) {
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
  printf(stdout, "%s small file test ok\n", fs_type);
}

void writetest1(const char *fs_type) {
  int i, fd, n;

  printf(stdout, "%s big files test\n", fs_type);

  fd = open("big", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "error: creat big failed!\n");
    exit(1);
  }

  for (i = 0; i < MAXFILE; i++) {
    ((int *)buf)[0] = i;
    if (write(fd, buf, 512) != 512) {
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
    i = read(fd, buf, 512);
    if (i == 0) {
      if (n != MAXFILE) {
        printf(stdout, "read only %d blocks from big", n);
        exit(0);
      }
      break;
    } else if (i != 512) {
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
  printf(stdout, "%s big files test ok\n", fs_type);
}

void createtest(const char *fs_type) {
  int i, fd;

  printf(stdout, "%s many creates, followed by unlink test\n", fs_type);

  name[0] = 'a';
  name[2] = '\0';
  for (i = 0; i < 52; i++) {
    name[1] = '0' + i;
    fd = open(name, O_CREATE | O_RDWR);
    close(fd);
  }
  name[0] = 'a';
  name[2] = '\0';
  for (i = 0; i < 52; i++) {
    name[1] = '0' + i;
    unlink(name);
  }
  printf(stdout, "%s many creates, followed by unlink; ok\n", fs_type);
}

void dirtest(const char *fs_type) {
  printf(stdout, "%s mkdir test\n", fs_type);

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
  printf(stdout, "%s mkdir test ok\n", fs_type);
}

int exectest(void) {
  printf(stdout, "exec test\n");
  if (exec("/echo", (const char **)echoargv) < 0) {
    printf(stdout, "exec echo failed\n");
    return 1;
  }
  return 0;
}

// simple fork and pipe read/write

void pipe1(const char *fs_type) {
  int fds[2], pid;
  int seq, i, n, cc, total;

  printf(stdout, "%s pipe1 test\n", fs_type);

  if (pipe(fds) != 0) {
    printf(stdout, "pipe() failed\n");
    exit(1);
  }
  pid = fork();
  seq = 0;
  if (pid == 0) {
    close(fds[0]);
    for (n = 0; n < 5; n++) {
      for (i = 0; i < 1033; i++) buf[i] = seq++;
      if (write(fds[1], buf, 1033) != 1033) {
        printf(stdout, "pipe1 oops 1\n");
        exit(1);
      }
    }
    exit(0);
  } else if (pid > 0) {
    close(fds[1]);
    total = 0;
    cc = 1;
    while ((n = read(fds[0], buf, cc)) > 0) {
      for (i = 0; i < n; i++) {
        if ((buf[i] & 0xff) != (seq++ & 0xff)) {
          printf(stdout, "pipe1 oops 2\n");
          return;
        }
      }
      total += n;
      cc = cc * 2;
      if (cc > sizeof(buf)) cc = sizeof(buf);
    }
    if (total != 5 * 1033) {
      printf(stdout, "pipe1 oops 3 total %d\n", total);
      exit(1);
    }
    close(fds[0]);
    wait(0);
  } else {
    printf(stdout, "fork() failed\n");
    exit(1);
  }
  printf(stdout, "%s pipe1 test ok\n", fs_type);
}

// meant to be run w/ at most two CPUs
void preempt(void) {
  int pid1, pid2, pid3;
  int pfds[2];

  printf(stdout, "preempt: ");
  pid1 = fork();
  if (pid1 == 0)
    for (;;) {
    }

  pid2 = fork();
  if (pid2 == 0)
    for (;;) {
    }

  pipe(pfds);
  pid3 = fork();
  if (pid3 == 0) {
    close(pfds[0]);
    if (write(pfds[1], "x", 1) != 1) printf(stdout, "preempt write error");
    close(pfds[1]);
    for (;;) {
    }
  }

  close(pfds[1]);
  if (read(pfds[0], buf, sizeof(buf)) != 1) {
    printf(stdout, "preempt read error");
    return;
  }
  close(pfds[0]);
  printf(stdout, "kill... ");
  kill(pid1);
  kill(pid2);
  kill(pid3);
  printf(stdout, "wait... ");
  wait(0);
  wait(0);
  wait(0);
  printf(stdout, "preempt ok\n");
}

// try to find any races between exit and wait
void exitwait(void) {
  int i, pid;

  for (i = 0; i < 100; i++) {
    pid = fork();
    if (pid < 0) {
      printf(stdout, "fork failed\n");
      return;
    }
    if (pid) {
      if (wait(0) != pid) {
        printf(stdout, "wait wrong pid\n");
        return;
      }
    } else {
      exit(0);
    }
  }
  printf(stdout, "exitwait ok\n");
}

void mem(void) {
  void *m1, *m2;
  int pid, ppid;

  printf(stdout, "mem test\n");
  ppid = getpid();
  if ((pid = fork()) == 0) {
    m1 = 0;
    while ((m2 = malloc(10001)) != 0) {
      *(char **)m2 = m1;
      m1 = m2;
    }
    while (m1) {
      m2 = *(char **)m1;
      free(m1);
      m1 = m2;
    }
    m1 = malloc(1024 * 20);
    if (m1 == 0) {
      printf(stdout, "couldn't allocate mem?!!\n");
      kill(ppid);
      exit(1);
    }
    free(m1);
    printf(stdout, "mem ok\n");
    exit(0);
  } else {
    wait(0);
  }
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void sharedfd(const char *fs_type) {
  int fd, pid, i, n, nc, np;
  char buf[10];

  printf(stdout, "%s sharedfd test\n", fs_type);

  unlink("sharedfd");
  fd = open("sharedfd", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "fstests: cannot open sharedfd for writing");
    return;
  }
  pid = fork();
  memset(buf, pid == 0 ? 'c' : 'p', sizeof(buf));
  for (i = 0; i < 1000; i++) {
    if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
      printf(stdout, "fstests: write sharedfd failed\n");
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
    printf(stdout, "fstests: cannot open sharedfd for reading\n");
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
  if (nc == 10000 && np == 10000) {
    printf(stdout, "%s sharedfd test ok\n", fs_type);
  } else {
    printf(stdout, "sharedfd oops %d %d\n", nc, np);
    exit(1);
  }
}

// four processes write different files at the same
// time, to test block allocation.
void fourfiles(const char *fs_type) {
  int fd, pid, i, j, n, total, pi;
  char *names[] = {"f0", "f1", "f2", "f3"};
  char *fname;

  printf(stdout, "%s fourfiles test\n", fs_type);

  for (pi = 0; pi < 4; pi++) {
    fname = names[pi];
    unlink(fname);

    pid = fork();
    if (pid < 0) {
      printf(stdout, "fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      fd = open(fname, O_CREATE | O_RDWR);
      if (fd < 0) {
        printf(stdout, "create failed\n");
        exit(1);
      }

      memset(buf, '0' + pi, 512);
      for (i = 0; i < 12; i++) {
        if ((n = write(fd, buf, 500)) != 500) {
          printf(stdout, "write failed %d\n", n);
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
          printf(stdout, "wrong char\n");
          exit(1);
        }
      }
      total += n;
    }
    close(fd);
    if (total != 12 * 500) {
      printf(stdout, "wrong length %d\n", total);
      exit(1);
    }
    unlink(fname);
  }

  printf(stdout, "%s fourfiles test ok\n", fs_type);
}

void createmanyfiles(const char *fs_type, uint number_of_files_to_create) {
  printf(stdout, "%s create many files\n", fs_type);
  char filename[100] = "file";
  for (int i = 0; i < number_of_files_to_create; i++) {
    // generate filename
    itoa(filename + 4, i);
    int fd = open(filename, O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(stdout, "create %s failed\n", filename);
      exit(1);
    }
    close(fd);
  }
  printf(stdout, "%s create many files ok\n", fs_type);
}

// four processes create and delete different files in same directory
void createdelete(const char *fs_type) {
  enum { N = 20 };
  int pid, i, fd, pi;
  char name[32];

  printf(stdout, "%s createdelete test\n", fs_type);

  for (pi = 0; pi < 4; pi++) {
    pid = fork();
    if (pid < 0) {
      printf(stdout, "fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      name[0] = 'p' + pi;
      name[2] = '\0';
      for (i = 0; i < N; i++) {
        name[1] = '0' + i;
        fd = open(name, O_CREATE | O_RDWR);
        if (fd < 0) {
          printf(stdout, "create failed\n");
          exit(1);
        }
        close(fd);
        if (i > 0 && (i % 2) == 0) {
          name[1] = '0' + (i / 2);
          if (unlink(name) < 0) {
            printf(stdout, "unlink failed\n");
            exit(1);
          }
        }
      }
      exit(0);
    }
  }

  for (pi = 0; pi < 4; pi++) {
    wait(0);
  }

  name[0] = name[1] = name[2] = 0;
  for (i = 0; i < N; i++) {
    for (pi = 0; pi < 4; pi++) {
      name[0] = 'p' + pi;
      name[1] = '0' + i;
      fd = open(name, 0);
      if ((i == 0 || i >= N / 2) && fd < 0) {
        printf(stdout, "oops createdelete %s didn't exist\n", name);
        exit(1);
      } else if ((i >= 1 && i < N / 2) && fd >= 0) {
        printf(stdout, "oops createdelete %s did exist\n", name);
        exit(1);
      }
      if (fd >= 0) close(fd);
    }
  }

  for (i = 0; i < N; i++) {
    for (pi = 0; pi < 4; pi++) {
      name[0] = 'p' + i;
      name[1] = '0' + i;
      unlink(name);
    }
  }

  printf(stdout, "%s createdelete test ok\n", fs_type);
}

// can I unlink a file and still read it?
void unlinkread(const char *fs_type) {
  int fd, fd1;

  printf(stdout, "%s unlinkread test\n", fs_type);
  fd = open("unlinkread", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "create unlinkread failed\n");
    exit(1);
  }
  write(fd, "hello", 5);
  close(fd);

  fd = open("unlinkread", O_RDWR);
  if (fd < 0) {
    printf(stdout, "open unlinkread failed\n");
    exit(1);
  }
  if (unlink("unlinkread") != 0) {
    printf(stdout, "unlink unlinkread failed\n");
    exit(1);
  }

  fd1 = open("unlinkread", O_CREATE | O_RDWR);
  write(fd1, "yyy", 3);
  close(fd1);

  if (read(fd, buf, sizeof(buf)) != 5) {
    printf(stdout, "unlinkread read failed");
    exit(1);
  }
  if (buf[0] != 'h') {
    printf(stdout, "unlinkread wrong data\n");
    exit(1);
  }
  if (write(fd, buf, 10) != 10) {
    printf(stdout, "unlinkread write failed\n");
    exit(1);
  }
  close(fd);
  unlink("unlinkread");
  printf(stdout, "%s unlinkread test ok\n", fs_type);
}

void linktest(const char *fs_type) {
  int fd;

  printf(stdout, "%s linktest\n", fs_type);

  unlink("lf1");
  unlink("lf2");

  fd = open("lf1", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "create lf1 failed\n");
    exit(1);
  }
  if (write(fd, "hello", 5) != 5) {
    printf(stdout, "write lf1 failed\n");
    exit(1);
  }
  close(fd);

  if (link("lf1", "lf2") < 0) {
    printf(stdout, "link lf1 lf2 failed\n");
    exit(1);
  }
  unlink("lf1");

  if (open("lf1", 0) >= 0) {
    printf(stdout, "unlinked lf1 but it is still there!\n");
    exit(1);
  }

  fd = open("lf2", 0);
  if (fd < 0) {
    printf(stdout, "open lf2 failed\n");
    exit(1);
  }
  if (read(fd, buf, sizeof(buf)) != 5) {
    printf(stdout, "read lf2 failed\n");
    exit(1);
  }
  close(fd);

  if (link("lf2", "lf2") >= 0) {
    printf(stdout, "link lf2 lf2 succeeded! oops\n");
    exit(1);
  }

  unlink("lf2");
  if (link("lf2", "lf1") >= 0) {
    printf(stdout, "link non-existant succeeded! oops\n");
    exit(1);
  }

  if (link(".", "lf1") >= 0) {
    printf(stdout, "link . lf1 succeeded! oops\n");
    exit(1);
  }

  printf(stdout, "%s linktest ok\n", fs_type);
}

// test concurrent create/link/unlink of the same file
void concreate(const char *fs_type) {
  char file[3];
  int i, pid, n, fd;
  char fa[40];
  struct {
    ushort inum;
    char name[14];
  } de;

  printf(stdout, "%s concreate test\n", fs_type);
  file[0] = 'C';
  file[2] = '\0';
  for (i = 0; i < 40; i++) {
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
        printf(stdout, "concreate create %s failed\n", file);
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
        printf(stdout, "concreate weird file %s\n", de.name);
        exit(1);
      }
      if (fa[i]) {
        printf(stdout, "concreate duplicate file %s\n", de.name);
        exit(1);
      }
      fa[i] = 1;
      n++;
    }
  }
  close(fd);

  if (n != 40) {
    printf(stdout, "concreate not enough files in directory listing\n");
    exit(1);
  }

  for (i = 0; i < 40; i++) {
    file[1] = '0' + i;
    pid = fork();
    if (pid < 0) {
      printf(stdout, "fork failed\n");
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

  printf(stdout, "%s concreate test ok\n", fs_type);
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
void linkunlink(const char *fs_type) {
  int pid, i;

  printf(stdout, "%s linkunlink test\n", fs_type);

  unlink("x");
  pid = fork();
  if (pid < 0) {
    printf(stdout, "fork failed\n");
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

  printf(stdout, "%s linkunlink test ok\n", fs_type);
}

// directory that uses indirect blocks
void bigdir(const char *fs_type) {
  int i, fd;
  char name[10];

  printf(stdout, "%s bigdir test\n", fs_type);
  unlink("bd");

  fd = open("bd", O_CREATE);
  if (fd < 0) {
    printf(stdout, "bigdir create failed\n");
    exit(1);
  }
  close(fd);

  for (i = 0; i < 500; i++) {
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if (link("bd", name) != 0) {
      printf(stdout, "bigdir link failed\n");
      exit(1);
    }
  }

  unlink("bd");
  for (i = 0; i < 500; i++) {
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if (unlink(name) != 0) {
      printf(stdout, "bigdir unlink failed");
      exit(1);
    }
  }

  printf(stdout, "%s bigdir test ok\n", fs_type);
}

void subdir(const char *fs_type) {
  int fd, cc;

  printf(stdout, "%s subdir test\n", fs_type);

  unlink("ff");
  if (mkdir("dd") != 0) {
    printf(stdout, "subdir mkdir dd failed\n");
    exit(1);
  }

  fd = open("dd/ff", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "create dd/ff failed\n");
    exit(1);
  }
  write(fd, "ff", 2);
  close(fd);

  if (unlink("dd") >= 0) {
    printf(stdout, "unlink dd (non-empty dir) succeeded!\n");
    exit(1);
  }

  if (mkdir("dd/dd") != 0) {
    printf(stdout, "subdir mkdir dd/dd failed\n");
    exit(1);
  }

  fd = open("dd/dd/ff", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "create dd/dd/ff failed\n");
    exit(1);
  }
  write(fd, "FF", 2);
  close(fd);

  fd = open("dd/dd/../ff", 0);
  if (fd < 0) {
    printf(stdout, "open dd/dd/../ff failed\n");
    exit(1);
  }
  cc = read(fd, buf, sizeof(buf));
  if (cc != 2 || buf[0] != 'f') {
    printf(stdout, "dd/dd/../ff wrong content\n");
    exit(1);
  }
  close(fd);

  if (link("dd/dd/ff", "dd/dd/ffff") != 0) {
    printf(stdout, "link dd/dd/ff dd/dd/ffff failed\n");
    exit(1);
  }

  if (unlink("dd/dd/ff") != 0) {
    printf(stdout, "unlink dd/dd/ff failed\n");
    exit(1);
  }
  if (open("dd/dd/ff", O_RDONLY) >= 0) {
    printf(stdout, "open (unlinked) dd/dd/ff succeeded\n");
    exit(1);
  }

  if (chdir("dd") != 0) {
    printf(stdout, "chdir dd failed\n");
    exit(1);
  }
  if (chdir("dd/../../dd") != 0) {
    printf(stdout, "chdir dd/../../dd failed\n");
    exit(1);
  }
  if (chdir("./..") != 0) {
    printf(stdout, "chdir ./.. failed\n");
    exit(1);
  }

  fd = open("dd/dd/ffff", 0);
  if (fd < 0) {
    printf(stdout, "open dd/dd/ffff failed\n");
    exit(1);
  }
  if (read(fd, buf, sizeof(buf)) != 2) {
    printf(stdout, "read dd/dd/ffff wrong len\n");
    exit(1);
  }
  close(fd);

  if (open("dd/dd/ff", O_RDONLY) >= 0) {
    printf(stdout, "open (unlinked) dd/dd/ff succeeded!\n");
    exit(1);
  }

  if (open("dd/ff/ff", O_CREATE | O_RDWR) >= 0) {
    printf(stdout, "create dd/ff/ff succeeded!\n");
    exit(1);
  }
  if (open("dd/xx/ff", O_CREATE | O_RDWR) >= 0) {
    printf(stdout, "create dd/xx/ff succeeded!\n");
    exit(1);
  }
  if (open("dd", O_CREATE) >= 0) {
    printf(stdout, "create dd succeeded!\n");
    exit(1);
  }
  if (open("dd", O_RDWR) >= 0) {
    printf(stdout, "open dd rdwr succeeded!\n");
    exit(1);
  }
  if (open("dd", O_WRONLY) >= 0) {
    printf(stdout, "open dd wronly succeeded!\n");
    exit(1);
  }
  if (link("dd/ff/ff", "dd/dd/xx") == 0) {
    printf(stdout, "link dd/ff/ff dd/dd/xx succeeded!\n");
    exit(1);
  }
  if (link("dd/xx/ff", "dd/dd/xx") == 0) {
    printf(stdout, "link dd/xx/ff dd/dd/xx succeeded!\n");
    exit(1);
  }
  if (link("dd/ff", "dd/dd/ffff") == 0) {
    printf(stdout, "link dd/ff dd/dd/ffff succeeded!\n");
    exit(1);
  }
  if (mkdir("dd/ff/ff") == 0) {
    printf(stdout, "mkdir dd/ff/ff succeeded!\n");
    exit(1);
  }
  if (mkdir("dd/xx/ff") == 0) {
    printf(stdout, "mkdir dd/xx/ff succeeded!\n");
    exit(1);
  }
  if (mkdir("dd/dd/ffff") == 0) {
    printf(stdout, "mkdir dd/dd/ffff succeeded!\n");
    exit(1);
  }
  if (unlink("dd/xx/ff") == 0) {
    printf(stdout, "unlink dd/xx/ff succeeded!\n");
    exit(1);
  }
  if (unlink("dd/ff/ff") == 0) {
    printf(stdout, "unlink dd/ff/ff succeeded!\n");
    exit(1);
  }
  if (chdir("dd/ff") == 0) {
    printf(stdout, "chdir dd/ff succeeded!\n");
    exit(1);
  }
  if (chdir("dd/xx") == 0) {
    printf(stdout, "chdir dd/xx succeeded!\n");
    exit(1);
  }

  if (unlink("dd/dd/ffff") != 0) {
    printf(stdout, "unlink dd/dd/ff failed\n");
    exit(1);
  }
  if (unlink("dd/ff") != 0) {
    printf(stdout, "unlink dd/ff failed\n");
    exit(1);
  }
  if (unlink("dd") == 0) {
    printf(stdout, "unlink non-empty dd succeeded!\n");
    exit(1);
  }
  if (unlink("dd/dd") < 0) {
    printf(stdout, "unlink dd/dd failed\n");
    exit(1);
  }
  if (unlink("dd") < 0) {
    printf(stdout, "unlink dd failed\n");
    exit(1);
  }

  printf(stdout, "%s subdir test ok\n", fs_type);
}

// test writes that are larger than the log.
void bigwrite(const char *fs_type) {
  int fd, sz;

  printf(stdout, "%s bigwrite test\n", fs_type);

  unlink("bigwrite");
  for (sz = 499; sz < 12 * 512; sz += 471) {
    fd = open("bigwrite", O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(stdout, "cannot create bigwrite\n");
      exit(1);
    }
    int i;
    for (i = 0; i < 2; i++) {
      int cc = write(fd, buf, sz);
      if (cc != sz) {
        printf(stdout, "write(%d) ret %d\n", sz, cc);
        exit(1);
      }
    }
    close(fd);
    unlink("bigwrite");
  }

  printf(stdout, "%s bigwrite test ok\n", fs_type);
}

void bigfile(const char *fs_type) {
  int fd, i, total, cc;

  printf(stdout, "%s bigfile test\n", fs_type);

  unlink("bigfile");
  fd = open("bigfile", O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(stdout, "cannot create bigfile");
    exit(1);
  }
  for (i = 0; i < 20; i++) {
    memset(buf, i, 600);
    if (write(fd, buf, 600) != 600) {
      printf(stdout, "write bigfile failed\n");
      exit(1);
    }
  }
  close(fd);

  fd = open("bigfile", 0);
  if (fd < 0) {
    printf(stdout, "cannot open bigfile\n");
    exit(1);
  }
  total = 0;
  for (i = 0;; i++) {
    cc = read(fd, buf, 300);
    if (cc < 0) {
      printf(stdout, "read bigfile failed\n");
      exit(1);
    }
    if (cc == 0) break;
    if (cc != 300) {
      printf(stdout, "short read bigfile\n");
      exit(1);
    }
    if (buf[0] != i / 2 || buf[299] != i / 2) {
      printf(stdout, "read bigfile wrong data\n");
      exit(1);
    }
    total += cc;
  }
  close(fd);
  if (total != 20 * 600) {
    printf(stdout, "read bigfile wrong total\n");
    exit(1);
  }
  unlink("bigfile");

  printf(stdout, "%s bigfile test ok\n", fs_type);
}

void fourteen(const char *fs_type) {
  int fd;

  // DIRSIZ is 14.
  printf(stdout, "%s fourteen test\n", fs_type);

  if (mkdir("12345678901234") != 0) {
    printf(stdout, "mkdir 12345678901234 failed\n");
    exit(1);
  }
  if (mkdir("12345678901234/123456789012345") != 0) {
    printf(stdout, "mkdir 12345678901234/123456789012345 failed\n");
    exit(1);
  }
  fd = open("123456789012345/123456789012345/123456789012345", O_CREATE);
  if (fd < 0) {
    printf(stdout,
           "create 123456789012345/123456789012345/123456789012345 failed\n");
    exit(1);
  }
  close(fd);
  fd = open("12345678901234/12345678901234/12345678901234", 0);
  if (fd < 0) {
    printf(stdout,
           "open 12345678901234/12345678901234/12345678901234 failed\n");
    exit(1);
  }
  close(fd);

  if (mkdir("12345678901234/12345678901234") == 0) {
    printf(stdout, "mkdir 12345678901234/12345678901234 succeeded!\n");
    exit(1);
  }
  if (mkdir("123456789012345/12345678901234") == 0) {
    printf(stdout, "mkdir 12345678901234/123456789012345 succeeded!\n");
    exit(1);
  }

  printf(stdout, "%s fourteen test ok\n", fs_type);
}

void rmdot(const char *fs_type) {
  printf(stdout, "%s rmdot test\n", fs_type);
  if (mkdir("dots") != 0) {
    printf(stdout, "mkdir dots failed\n");
    exit(1);
  }
  if (chdir("dots") != 0) {
    printf(stdout, "chdir dots failed\n");
    exit(1);
  }
  if (unlink(".") == 0) {
    printf(stdout, "rm . worked!\n");
    exit(1);
  }
  if (unlink("..") == 0) {
    printf(stdout, "rm .. worked!\n");
    exit(1);
  }
  if (chdir("..") != 0) {
    printf(stdout, "chdir .. failed\n");
    exit(1);
  }
  if (unlink("dots/.") == 0) {
    printf(stdout, "unlink dots/. worked!\n");
    exit(1);
  }
  if (unlink("dots/..") == 0) {
    printf(stdout, "unlink dots/.. worked!\n");
    exit(1);
  }
  if (unlink("dots") != 0) {
    printf(stdout, "unlink dots failed!\n");
    exit(1);
  }
  printf(stdout, "%s rmdot test ok\n", fs_type);
}

void dirfile(const char *fs_type) {
  int fd;

  printf(stdout, "%s dir vs file\n", fs_type);

  fd = open("dirfile", O_CREATE);
  if (fd < 0) {
    printf(stdout, "create dirfile failed\n");
    exit(1);
  }
  close(fd);
  if (chdir("dirfile") == 0) {
    printf(stdout, "chdir dirfile succeeded!\n");
    exit(1);
  }
  fd = open("dirfile/xx", 0);
  if (fd >= 0) {
    printf(stdout, "create dirfile/xx succeeded!\n");
    exit(1);
  }
  fd = open("dirfile/xx", O_CREATE);
  if (fd >= 0) {
    printf(stdout, "create dirfile/xx succeeded!\n");
    exit(1);
  }
  if (mkdir("dirfile/xx") == 0) {
    printf(stdout, "mkdir dirfile/xx succeeded!\n");
    exit(1);
  }
  if (unlink("dirfile/xx") == 0) {
    printf(stdout, "unlink dirfile/xx succeeded!\n");
    exit(1);
  }
  if (link("README", "dirfile/xx") == 0) {
    printf(stdout, "link to dirfile/xx succeeded!\n");
    exit(1);
  }
  if (unlink("dirfile") != 0) {
    printf(stdout, "unlink dirfile failed!\n");
    exit(1);
  }

  fd = open(".", O_RDWR);
  if (fd >= 0) {
    printf(stdout, "open . for writing succeeded!\n");
    exit(1);
  }
  fd = open(".", 0);
  if (write(fd, "x", 1) > 0) {
    printf(stdout, "write . succeeded!\n");
    exit(1);
  }
  close(fd);

  printf(stdout, "%s dir vs file ok\n", fs_type);
}

// test that iput() is called at the end of _namei()
void iref(const char *fs_type) {
  int i, fd, depth;

  printf(stdout, "%s empty file name\n", fs_type);

  // the 60 is NINODE
  depth = 60;
  for (i = 0; i < depth + 1; i++) {
    if (mkdir("irefd") != 0) {
      printf(stdout, "mkdir irefd failed\n");
      exit(1);
    }
    if (chdir("irefd") != 0) {
      printf(stdout, "chdir irefd failed\n");
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

  // Get back to the initial state
  for (i = 0; i < depth + 1; i++) {
    chdir("..");
  }

  printf(stdout, "%s empty file name ok\n", fs_type);
}

// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void forktest(void) {
  int n, pid;

  printf(stdout, "fork test\n");

  for (n = 0; n < 1000; n++) {
    pid = fork();
    if (pid < 0) break;
    if (pid == 0) exit(0);
  }

  if (n == 1000) {
    printf(stdout, "fork claimed to work 1000 times!\n");
    exit(0);
  }

  for (; n > 0; n--) {
    if (wait(0) < 0) {
      printf(stdout, "wait stopped early\n");
      exit(1);
    }
  }

  if (wait(0) != -1) {
    printf(stdout, "wait got too many\n");
    exit(1);
  }

  printf(stdout, "fork test OK\n");
}

void sbrktest(void) {
  int fds[2], pid, pids[10], ppid;
  char *a, *b, *c, *lastaddr, *oldbrk, *p, scratch;
  uint amt;

  printf(stdout, "sbrk test\n");
  oldbrk = sbrk(0);

  // can one sbrk() less than a page?
  a = sbrk(0);
  int i;
  for (i = 0; i < 5000; i++) {
    b = sbrk(1);
    if (b != a) {
      printf(stdout, "sbrk test failed %d %x %x\n", i, a, b);
      exit(1);
    }
    *b = 1;
    a = b + 1;
  }
  pid = fork();
  if (pid < 0) {
    printf(stdout, "sbrk test fork failed\n");
    exit(1);
  }
  c = sbrk(1);
  c = sbrk(1);
  if (c != a + 1) {
    printf(stdout, "sbrk test failed post-fork\n");
    exit(1);
  }
  if (pid == 0) exit(0);
  wait(0);

  // can one grow address space to something big?
#define BIG (100 * 1024 * 1024)
  a = sbrk(0);
  amt = (BIG) - (uint)a;
  p = sbrk(amt);
  if (p != a) {
    printf(stdout,
           "sbrk test failed to grow big address space; enough phys mem?\n");
    exit(1);
  }
  lastaddr = (char *)(p + amt - 1);
  *lastaddr = 99;

  // can one de-allocate?
  a = sbrk(0);
  c = sbrk(-4096);
  if (c == (char *)0xffffffff) {
    printf(stdout, "sbrk could not deallocate\n");
    exit(1);
  }
  c = sbrk(0);
  if (c != a - 4096) {
    printf(stdout, "sbrk deallocation produced wrong address, a %x c %x\n", a,
           c);
    exit(1);
  }

  // can one re-allocate that page?
  a = sbrk(0);
  c = sbrk(4096);
  if (c != a || sbrk(0) != a + 4096) {
    printf(stdout, "sbrk re-allocation failed, a %x c %x\n", a, c);
    exit(1);
  }
  if (*lastaddr == 99) {
    // should be zero
    printf(stdout, "sbrk de-allocation didn't really deallocate\n");
    exit(1);
  }

  a = sbrk(0);
  c = sbrk(-(sbrk(0) - oldbrk));
  if (c != a) {
    printf(stdout, "sbrk downsize failed, a %x c %x\n", a, c);
    exit(1);
  }

  // can we read the kernel's memory?
  for (a = (char *)(KERNBASE); a < (char *)(KERNBASE + 2000000); a += 50000) {
    ppid = getpid();
    pid = fork();
    if (pid < 0) {
      printf(stdout, "fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      printf(stdout, "oops could read %x = %x\n", a, *a);
      kill(ppid);
      exit(1);
    }
    wait(0);
  }

  // if we run the system out of memory, does it clean up the last
  // failed allocation?
  if (pipe(fds) != 0) {
    printf(stdout, "pipe() failed\n");
    exit(1);
  }
  for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
    if ((pids[i] = fork()) == 0) {
      // allocate a lot of memory
      sbrk(BIG - (uint)sbrk(0));
      write(fds[1], "x", 1);
      // sit around until killed
      for (;;) sleep(1000);
    }
    if (pids[i] != -1) read(fds[0], &scratch, 1);
  }
  // if those failed allocations freed up the pages they did allocate,
  // we'll be able to allocate here
  c = sbrk(4096);
  for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
    if (pids[i] == -1) continue;
    kill(pids[i]);
    wait(0);
  }
  if (c == (char *)0xffffffff) {
    printf(stdout, "failed sbrk leaked memory\n");
    exit(1);
  }

  if (sbrk(0) > oldbrk) sbrk(-(sbrk(0) - oldbrk));

  printf(stdout, "sbrk test OK\n");
}

void validateint(int *p) {
  int res;
  asm("mov %%esp, %%ebx\n\t"
      "mov %3, %%esp\n\t"
      "int %2\n\t"
      "mov %%ebx, %%esp"
      : "=a"(res)
      : "a"(SYS_sleep), "n"(T_SYSCALL), "c"(p)
      : "ebx");
}

void validatetest(void) {
  int hi, pid;
  uint p;

  printf(stdout, "validate test\n");
  hi = 1100 * 1024;

  for (p = 0; p <= (uint)hi; p += 4096) {
    if ((pid = fork()) == 0) {
      // try to crash the kernel by passing in a badly placed integer
      validateint((int *)p);
      exit(1);
    }
    sleep(0);
    sleep(0);
    kill(pid);
    wait(0);

    // try to crash the kernel by passing in a bad string pointer
    if (link("nosuchfile", (char *)p) != -1) {
      printf(stdout, "link should not succeed\n");
      exit(1);
    }
  }

  printf(stdout, "validate ok\n");
}

// does unintialized data start out zero?
char uninit[10000];
void bsstest(void) {
  int i;

  printf(stdout, "bss test\n");
  for (i = 0; i < sizeof(uninit); i++) {
    if (uninit[i] != '\0') {
      printf(stdout, "bss test failed\n");
      exit(1);
    }
  }
  printf(stdout, "bss test ok\n");
}

// does exec return an error if the arguments
// are larger than a page? or does it write
// below the stack and wreck the instructions/data?
void bigargtest(void) {
  int pid, fd;

  unlink("bigarg-ok");
  pid = fork();
  if (pid == 0) {
    static char *args[MAXARG];
    int i;
    for (i = 0; i < MAXARG - 1; i++)
      args[i] =
          "bigargs test: failed\n                                              "
          "                                                                    "
          "                                                                    "
          "                 ";
    args[MAXARG - 1] = 0;
    printf(stdout, "bigarg test\n");
    exec("/echo", (const char **)args);
    printf(stdout, "bigarg test ok\n");
    fd = open("bigarg-ok", O_CREATE);
    close(fd);
    exit(0);
  } else if (pid < 0) {
    printf(stdout, "bigargtest: fork failed\n");
    exit(1);
  }
  wait(0);
  fd = open("bigarg-ok", 0);
  if (fd < 0) {
    printf(stdout, "bigarg test failed!\n");
    exit(1);
  }
  close(fd);
  unlink("bigarg-ok");
}

// what happens when the file system runs out of blocks?
// answer: balloc panics, so this test is not useful.
void fsfull() {
  int nfiles;
  int fsblocks = 0;

  printf(stdout, "fsfull test\n");

  for (nfiles = 0;; nfiles++) {
    char name[64];
    name[0] = 'f';
    name[1] = '0' + nfiles / 1000;
    name[2] = '0' + (nfiles % 1000) / 100;
    name[3] = '0' + (nfiles % 100) / 10;
    name[4] = '0' + (nfiles % 10);
    name[5] = '\0';
    printf(stdout, "writing %s\n", name);
    int fd = open(name, O_CREATE | O_RDWR);
    if (fd < 0) {
      printf(stdout, "open %s failed\n", name);
      break;
    }
    int total = 0;
    while (1) {
      int cc = write(fd, buf, 512);
      if (cc < 512) break;
      total += cc;
      fsblocks++;
    }
    printf(stdout, "wrote %d bytes\n", total);
    close(fd);
    if (total == 0) break;
  }

  while (nfiles >= 0) {
    char name[64];
    name[0] = 'f';
    name[1] = '0' + nfiles / 1000;
    name[2] = '0' + (nfiles % 1000) / 100;
    name[3] = '0' + (nfiles % 100) / 10;
    name[4] = '0' + (nfiles % 10);
    name[5] = '\0';
    unlink(name);
    nfiles--;
  }

  printf(stdout, "fsfull test finished\n");
}

void uio() {
#define RTC_ADDR 0x70
#define RTC_DATA 0x71

  ushort port = 0;
  uchar val = 0;
  int pid;

  printf(stdout, "uio test\n");
  pid = fork();
  if (pid == 0) {
    port = RTC_ADDR;
    val = 0x09;  // year

    // http://wiki.osdev.org/Inline_Assembly/Examples
    asm volatile("outb %0,%1" ::"a"(val), "d"(port));
    port = RTC_DATA;
    asm volatile("inb %1,%0" : "=a"(val) : "d"(port));
    printf(stdout, "uio: uio succeeded; test FAILED\n");
    exit(1);
  } else if (pid < 0) {
    printf(stdout, "fork failed\n");
    exit(1);
  }
  wait(0);
  printf(stdout, "uio test done\n");
}

void argptest() {
  int fd;
  fd = open("/init", O_RDONLY);
  if (fd < 0) {
    printf(stderr, "open failed\n");
    exit(1);
  }
  read(fd, sbrk(0) - 1, -1);
  close(fd);
  printf(stdout, "arg test passed\n");
}

unsigned long randstate = 1;
unsigned int rand() {
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}

void printftest() {
  int num_chars;
  int exit_error = -1;

  enum printf_test_strings {
    HELLO_WORLD,
    INTEGER_TEST,
    POINTER_TEST,
    HEXA_TEST,
    PERCENT_TEST,
    UNKNOWN_TYPE_TEST,
    STRING_NULL_TEST,
    CHAR_TEST
  };
  const char *test_fmts[] = {
      "hello world!?#~$&*()-=+\\][{}|/\n", "check integer %d number\n",
      "check pointer address %p\n",        "check hexa number 0x%x\n",
      "check double percent %%\n",         "unknown type %z\n",
      "string is null test %s\n"};
  const char *test_strings[] = {
      "hello world!?#~$&*()-=+\\][{}|/\n", "check integer 53 number\n",
      "check pointer address DF94\n",      "check hexa number 0x35\n",
      "check double percent %\n",          "unknown type %z\n",
      "string is null test (null)\n"};

  printf(stdout, "printftest starting\n");

  num_chars = printf(stdout, test_fmts[HELLO_WORLD]);
  if (num_chars != strlen(test_fmts[HELLO_WORLD])) {
    printf(stderr,
           "printftest failed on HELLO_WORLD string. Expected size = %d, got = "
           "%d\n",
           num_chars, strlen(test_fmts[HELLO_WORLD]));
    exit(exit_error);
  }

  num_chars = printf(stdout, test_fmts[INTEGER_TEST], 53);
  if (num_chars != strlen(test_strings[INTEGER_TEST])) {
    printf(stderr,
           "printftest failed on INTEGER_TEST string. Expected size = %d, got "
           "= %d\n",
           num_chars, strlen(test_strings[INTEGER_TEST]));
    exit(exit_error);
  }

  num_chars = printf(stdout, test_fmts[POINTER_TEST], test_fmts);
  if (num_chars != strlen(test_strings[POINTER_TEST])) {
    printf(stderr,
           "printftest failed on POINTER_TEST string. Expected size = %d, got "
           "= %d\n",
           num_chars, strlen(test_strings[POINTER_TEST]));
    exit(exit_error);
  }

  num_chars = printf(stdout, test_fmts[HEXA_TEST], 53);
  if (num_chars != strlen(test_strings[HEXA_TEST])) {
    printf(
        2,
        "printftest failed on HEXA_TEST string. Expected size = %d, got = %d\n",
        num_chars, strlen(test_strings[HEXA_TEST]));
    exit(exit_error);
  }

  num_chars = printf(stdout, test_fmts[PERCENT_TEST]);
  if (num_chars != strlen(test_strings[PERCENT_TEST])) {
    printf(stderr,
           "printftest failed on PERCENT_TEST string. Expected size = %d, got "
           "= %d\n",
           num_chars, strlen(test_strings[PERCENT_TEST]));
    exit(exit_error);
  }

  num_chars = printf(stdout, test_fmts[UNKNOWN_TYPE_TEST]);
  if (num_chars != strlen(test_strings[UNKNOWN_TYPE_TEST])) {
    printf(stderr,
           "printftest failed on UNKNOWN_TYPE_TEST string. Expected size = %d, "
           "got = %d\n",
           num_chars, strlen(test_strings[UNKNOWN_TYPE_TEST]));
    exit(exit_error);
  }

  num_chars = printf(stdout, test_fmts[STRING_NULL_TEST], 0);
  if (num_chars != strlen(test_strings[STRING_NULL_TEST])) {
    printf(stderr,
           "printftest failed on STRING_NULL_TEST string. Expected size = %d, "
           "got = %d\n",
           num_chars, strlen(test_strings[STRING_NULL_TEST]));
    exit(exit_error);
  }

  printf(stdout, "printftest passed\n");
}

void exitrctest() {
  int exit_code = 501;
  int pid = fork();
  if (pid == 0) {
    exit(exit_code);
  }
  int wstatus;
  wait(&wstatus);
  if (WEXITSTATUS(wstatus) != exit_code) {
    printf(stderr, "exitrctest: ERROR - failed to get correct exit status\n");
    exit(1);
  } else {
    printf(stdout, "exitrctest ok\n");
  }
}

void memtest() {
  if (kmemtest()) {
    printf(stderr, "memtest: memory corruption\n");
    exit(1);
  }
  printf(stdout, "memtest: memory ok\n");
}

// Assumes the current directory is inside the tested fs mount
void all_fs_tests(const char *fs_type) {
  createdelete(fs_type);
  linkunlink(fs_type);
  concreate(fs_type);
  fourfiles(fs_type);
  sharedfd(fs_type);
  createmanyfiles(fs_type, 100);
  bigwrite(fs_type);
  opentest(fs_type);
  openexclusivetest(fs_type);
  writetest(fs_type);
  writetest1(fs_type);
  createtest(fs_type);
  openiputtest(fs_type);
  exitiputtest(fs_type);
  iputtest(fs_type);
  pipe1(fs_type);
  rmdot(fs_type);
  fourteen(fs_type);
  bigfile(fs_type);
  subdir(fs_type);
  linktest(fs_type);
  unlinkread(fs_type);
  dirfile(fs_type);
  iref(fs_type);
  bigdir(fs_type);  // slow
}

void nativefs_all_tests(void) {
  printf(stdout, "nativefs all tests\n");

  unlink("nativefs_dir");
  if (mkdir("nativefs_dir") < 0) {
    printf(stdout, "mkdir native_fs_dir failed\n");
    exit(1);
  }
  if (chdir("nativefs_dir") < 0) {
    printf(stdout, "chdir native_fs_dir failed\n");
    exit(1);
  }

  all_fs_tests("nativefs");

  if (chdir("..") < 0) {
    printf(stdout, "chdir .. failed\n");
    exit(1);
  }
  printf(stdout, "nativefs all tests ok\n");
}

void objfs_all_tests(void) {
  printf(stdout, "objfs all tests\n");

  unlink("objfs_dir");
  if (mkdir("objfs_dir") < 0) {
    printf(stdout, "mkdir objfs_dir failed\n");
    exit(1);
  }
  if (mount(0, "objfs_dir", "objfs") != 0) {
    printf(stdout, "failed to mount objfs to /objfs_dir\n");
    exit(1);
  }
  if (chdir("objfs_dir") < 0) {
    printf(stdout, "chdir objfs_dir failed\n");
    exit(1);
  }

  all_fs_tests("objfs");

  if (chdir("..") < 0) {
    printf(stdout, "chdir ..failed\n");
    exit(1);
  }

  // TODO(SM): insert these lines only after ".." dir entry of mount is
  //           fixed to point to the right parent dir
#if 0
  if (umount("objfs_dir") < 0) {
    printf(stdout, "umount objfs failed\n");
    exit(1);
  }
#endif

  printf(stdout, "objfs all tests ok\n");
}

int main(int argc, char *argv[]) {
  printf(stdout, "usertests starting\n");

  if (open("usertests.ran", 0) >= 0) {
    printf(stdout, "already ran user tests -- rebuild fs.img\n");
    exit(1);
  }
  close(open("usertests.ran", O_CREATE));

  printftest();

  argptest();

  nativefs_all_tests();
  objfs_all_tests();

  bigargtest();
  bigargtest();
  bsstest();
  sbrktest();
  validatetest();

  mem();
  preempt();
  exitwait();

  forktest();
  memtest();

  uio();
  exitrctest();
  return (exectest());
}
