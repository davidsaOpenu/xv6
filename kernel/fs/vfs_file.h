#ifndef XV6_VFS_FILE_H
#define XV6_VFS_FILE_H

#include "defs.h"
#include "device/device.h"
#include "kvector.h"
#include "param.h"
#include "sleeplock.h"
#include "stat.h"
#include "vfs_fs.h"

struct vfs_file;

struct vfs_file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_CG, FD_PROC } type;
  int ref;  // reference count
  char readable;
  char writable;
  uint off;
  union {
    // FD_PIPE
    struct pipe *pipe;

    // FD_INODE
    struct {
      struct vfs_inode *ip;
      struct mount *mnt;
    };

    // FD_CG
    struct {
      struct cgroup *cgp;
      char cgfilename[MAX_CGROUP_FILE_NAME_LENGTH];
      union {
        // cpu
        union {
          struct {
            char active;
            int usage_usec;
            int user_usec;
            int system_usec;
            int nr_periods;
            int nr_throttled;
            int throttled_usec;
          } stat;
          struct {
            int weight;
          } weight;
          struct {
            int max;
            int period;
          } max;
        } cpu;
        // pid
        union {
          struct {
            char active;
            int max;
          } max;
        } pid;
        // cpu_set
        union {
          struct {
            char active;
            int cpu_id;
          } set;
        } cpu_s;
        // freezer
        union {
          struct {
            int frozen;
          } freezer;
        } frz;
        // IO
        union {
          struct cgroup_io_device_statistics_s *devices_stats[NDEV];
        } io;
        // memory
        union {
          struct {
            char active;
            uint file_dirty;
            uint file_dirty_aggregated;
            uint pgfault;
            uint pgmajfault;
            uint kernel;
          } stat;
          struct {
            char active;
            unsigned int max;
          } max;
          struct {
            char active;
            unsigned int min;
          } min;
        } mem;
      };
    };

    // FD_PROC
    struct {
      int filetype;
      int filename_const;
      char filename[MAX_PROC_FILE_NAME_LENGTH];
      union {
        uint mem;
        struct mount_list *mount_entry;
        struct device devs[NMAXDEVS];
      } proc;
      uint count; /* Useful to count mount entries/devs, etc.. */
    };
  };
};

struct ftable_s {
  struct spinlock lock;
  struct vfs_file file[NFILE];
};

extern struct ftable_s ftable;

struct inode_operations {
  int (*dirlink)(struct vfs_inode *, char *, uint);
  struct vfs_inode *(*dirlookup)(struct vfs_inode *, char *, uint *);
  struct vfs_inode *(*idup)(struct vfs_inode *);
  void (*ilock)(struct vfs_inode *);
  void (*iput)(struct vfs_inode *);
  void (*iunlock)(struct vfs_inode *);
  void (*iunlockput)(struct vfs_inode *);
  void (*iupdate)(struct vfs_inode *);
  int (*readi)(struct vfs_inode *, uint, uint, vector *);
  void (*stati)(struct vfs_inode *, struct stat *);
  int (*writei)(struct vfs_inode *, char *, uint, uint);
  int (*isdirempty)(struct vfs_inode *);
};

// in-memory copy of an inode
struct vfs_inode {
  struct vfs_superblock *sb;  // The vfs_superblock that this inode belongs to
  uint inum;                  // Inode number
  int ref;                    // Reference count
  struct sleeplock lock;      // protects everything below here
  int valid;                  // inode has been read from disk?
  short type;                 // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  const struct inode_operations *i_op;
  struct mount *mnt;  // if this inode is a mount point, this is the mount
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct vfs_inode *, int, vector *dstvector);
  int (*write)(struct vfs_inode *, char *, int);
  int (*stat)(int, struct dev_stat *);
};

/* device statistics structure which defines what info every device
   should supply.
*/
struct dev_stat {
  /* number of read IO operation made on the device */
  uint rios;
  /* number of write IO operation made on the device */
  uint wios;
  /* Total bytes read from device */
  uint rbytes;
  /* Total bytes written to device */
  uint wbytes;
};

extern struct devsw devsw[];

#endif /* XV6_VFS_FILE_H */
