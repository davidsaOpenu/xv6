#ifndef XV6_STAT_H
#define XV6_STAT_H

#include "types.h"

typedef enum file_type {
  T_DIR = 1,       // Directory
  T_FILE = 2,      // File
  T_DEV = 3,       // Device
  T_CGFILE = 4,    // Cgroup file
  T_CGDIR = 5,     // Cgroup directory
  T_PROCFILE = 6,  // Proc file
  T_PROCDIR = 7    // Proc directory
} file_type;

struct stat {
  file_type type;  // Type of file
  int dev;         // File system's disk device
  uint ino;        // Inode number
  short nlink;     // Number of links to file
  uint size;       // Size of file in bytes
};

#endif /* XV6_STAT_H */
