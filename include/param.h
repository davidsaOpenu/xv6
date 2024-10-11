#ifndef XV6_PARAM_H
#define XV6_PARAM_H

#define NPROC 64                   // maximum number of processes
#define KSTACKSIZE 4096            // size of per-process kernel stack
#define NCPU 8                     // maximum number of CPUs
#define NOFILE 16                  // open files per process
#define NFILE 200                  // open files per system
#define NINODE 60                  // maximum number of active i-nodes
#define NDEV 10                    // maximum major device number
#define MAX_TTY 4                  // maximum minor tty number
#define ROOTDEV 1                  // device number of file system root disk
#define MAXARG 32                  // max exec arguments
#define MAXOPBLOCKS 10             // max # of blocks any FS op writes
#define LOGSIZE (MAXOPBLOCKS * 3)  // max data blocks in on-disk log
#define NBUF 200                   // size of system disks block cache
#define FSSIZE 2400                // size of file system in blocks
#define INT_FSSIZE 80              // size of internal file systems in blocks
#define NNAMESPACE 20              // maximum number of namespaces
#define MAX_PATH_LENGTH 512        // maximum path length allowed
#define MAX_CGROUP_FILE_NAME_LENGTH \
  64  // maximum allowed length of cgroup file name
#define MAX_PROC_FILE_NAME_LENGTH \
  64                                 // maximum allowed length of proc file name
#define MAX_CGROUP_CPU_WEIGHT 10000  // maximum allowed cpu weight
#define MIN_CGROUP_CPU_WEIGHT 1      // minimum allowed cpu weight
#define DEFAULT_CGROUP_CPU_WEIGHT 100  // default cpu weight
#define DEFAULT_CPU_ACCOUNT_PERIOD_100MS \
  1 * 100 * 1000  // default cpu account period - 100ms

#endif /* XV6_PARAM_H */
