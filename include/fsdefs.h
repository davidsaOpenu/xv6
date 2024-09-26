#ifndef XV6_FSDEFS_H
#define XV6_FSDEFS_H

#include "types.h"
// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOTINO 1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct native_superblock {
  uint size;        // Size of file system image (blocks)
  uint nblocks;     // Number of data blocks
  uint nlog;        // Number of log blocks
  uint logstart;    // Block number of first log block
  uint inodestart;  // Block number of first inode block
  uint bmapstart;   // Block number of first free map block
  uint ninodes;     // Number of inodes.
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// Base structure for on-disk inode.
// this structure is used to save common details about inode
// when it is loaded/stored from disk.
struct base_dinode {
  short type;   // File type
  short major;  // Major device number (T_DEV only)
  short minor;  // Minor device number (T_DEV only)
  short nlink;  // Number of links to inode in file system
};

// On-disk inode structure for the native filesystem
// that also keeps one to one correspondence between On-disk inode structure and
// it's base_dinode counterpart
struct native_dinode {
  struct base_dinode base_dinode;
  uint size;                // Size of file (bytes)
  uint addrs[NDIRECT + 1];  // Data block addresses
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct native_dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + (sb).inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + (sb).bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// Directory entry
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#endif /* XV6_FSDEFS_H */
