#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "fs.h"
#include "param.h"
#include "stat.h"
#include "types.h"

#ifndef static_assert
#define static_assert(a, b) \
  do {                      \
    switch (0)              \
    case 0:                 \
    case (a):;              \
  } while (0)
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;    // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks;  // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// convert to intel byte order
void printusageexit(void) {
  fprintf(stderr, "Usage: mkfs fs.img <is_internal (0/1)> files...\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if (argc < 3) {
    printusageexit();
  }

  if (strlen(argv[2]) != 1 || (argv[2][0] != '0' && argv[2][0] != '1')) {
    printusageexit();
  }

  int is_internal = argv[2][0] == '1';

  int fssize = is_internal ? INT_FSSIZE : FSSIZE;
  int nbitmap = fssize / (BSIZE * 8) + 1;

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fsfd < 0) {
    perror(argv[1]);
    exit(1);
  }

  // 1 fs block = 1 disk sector
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = fssize - nmeta;

  sb.size = (uint)fssize;
  sb.nblocks = (uint)nblocks;
  sb.vfs_sb.ninodes = (uint)NINODES;
  sb.nlog = (uint)nlog;
  sb.logstart = (uint)2;
  sb.inodestart = (uint)(2 + nlog);
  sb.bmapstart = (uint)(2 + nlog + ninodeblocks);

  printf(
      "nmeta %d (boot, super, log blocks %d inode blocks %d, bitmap blocks %d) "
      "blocks %d total %d\n",
      nmeta, nlog, ninodeblocks, nbitmap, nblocks, fssize);

  freeblock = nmeta;  // the first free block that we can allocate

  for (i = 0; i < fssize; i++) wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);

  bzero(&de, sizeof(de));
  de.inum = (ushort)rootino;
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = (ushort)rootino;
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  for (i = 3; i < argc; i++) {
    assert(index(argv[i], '/') == 0);

    if ((fd = open(argv[i], 0)) < 0) {
      perror(argv[i]);
      exit(1);
    }

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if (argv[i][0] == '_') ++argv[i];

    inum = ialloc(T_FILE);

    bzero(&de, sizeof(de));
    de.inum = (ushort)inum;
    strncpy(de.name, argv[i], DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    while ((cc = read(fd, buf, sizeof(buf))) > 0) iappend(inum, buf, cc);

    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = (uint)din.size;
  off = ((off / BSIZE) + 1) * BSIZE;
  din.size = (uint)off;
  winode(rootino, &din);

  balloc(freeblock);

  exit(0);
}

void wsect(uint sec, void *buf) {
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (write(fsfd, buf, BSIZE) != BSIZE) {
    perror("write");
    exit(1);
  }
}

void winode(uint inum, struct dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode *)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void rinode(uint inum, struct dinode *ip) {
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode *)buf) + (inum % IPB);
  *ip = *dip;
}

void rsect(uint sec, void *buf) {
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
    perror("lseek");
    exit(1);
  }
  if (read(fsfd, buf, BSIZE) != BSIZE) {
    perror("read");
    exit(1);
  }
}

uint ialloc(ushort type) {
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.vfs_dinode.type = (ushort)type;
  din.vfs_dinode.nlink = (ushort)1;
  din.size = (uint)0;
  winode(inum, &din);
  return inum;
}

void balloc(int used) {
  uchar buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < BSIZE * 8);
  bzero(buf, BSIZE);
  for (i = 0; i < used; i++) {
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  printf("balloc: write bitmap block at sector 0x%x\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n) {
  char *p = (char *)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = (uint)din.size;
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while (n > 0) {
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if (fbn < NDIRECT) {
      if ((uint)din.addrs[fbn] == 0) {
        din.addrs[fbn] = (uint)freeblock++;
      }
      x = (uint)din.addrs[fbn];
    } else {
      if ((uint)din.addrs[NDIRECT] == 0) {
        din.addrs[NDIRECT] = (uint)freeblock++;
      }
      rsect((uint)din.addrs[NDIRECT], (char *)indirect);
      if (indirect[fbn - NDIRECT] == 0) {
        indirect[fbn - NDIRECT] = (uint)freeblock++;
        wsect((uint)din.addrs[NDIRECT], (char *)indirect);
      }
      x = (uint)indirect[fbn - NDIRECT];
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = (uint)off;
  winode(inum, &din);
}
