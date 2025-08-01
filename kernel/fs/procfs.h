/* Proc filesystem functions. */

#ifndef XV6_PROCFS_H
#define XV6_PROCFS_H

#include "proc.h"
#include "stat.h"

#define MAX_BUF (4096)

#define PROCFS_MEM "mem"
#define PROCFS_MOUNTS "mounts"
#define PROCFS_DEVICES "devices"
#define PROCFS_CACHE "cache"
#define PROCFS_KMEMTEST "kmemtest"

/* /proc/mounts strings. */
#define MOUNTS_TITLE "Mounts:"
#define MOUNTS_MOUNT ": Mount "
#define MOUNTS_ATTACHED_TO " attached to "
#define MOUNTS_CHILD_OF ", child of "
#define MOUNTS_WITH_REF ", with ref "

/* /proc/devices strings. */
#define DEVICES_DEVICE "Device "
#define DEVICES_BACKED_BY_INODE " back by inode "
#define DEVICES_WITH_REF " with ref "

/* /proc/cache strings. */
#define CACHE_ENABLED "1\n"
#define CACHE_DISABLED "0\n"
#define CACHE_STATUS_LEN (2)

/* /proc/kmemtest strings. */
#define KMEMTEST_TITLE "Free Memory Pages:"
#define KMEMTEST_COUNTER "  counter: "
#define KMEMTEST_LIST "  list:    "
#define KMEMTEST_ERRORS "  errors:  "

typedef enum proc_file_name_e {
  NONE = -1,
  PROC_FILE_NAME_START = 0,

  PROC_MEM,
  PROC_MOUNTS,
  PROC_DEVICES,
  PROC_CACHE,
  PROC_KMEMTEST,

  PROC_FILE_NAME_END,
  NON_WRITABLE,
} proc_file_name_t;

/**
 * This function opens a proc filesystem file or directory.
 * Receives proc_file_type parameter "type", string parameter "filename", int
 * parameter "omode". "type" is a proc filesystem file type from stat.h that
 * denotes whether we are opening a file or a directory. "filename" is a string
 * containing the filename. "omode" is the opening mode. Same as with regular
 * files. Return values: -1 on failure. file descriptor of the new open file on
 * success. currently supports opening: 1)    "mem" 2)    "mounts" 3) "device"
 *    4)    proc directories
 */
int unsafe_proc_open(int filetype, char* filename, int omode);

/**
 * This function reads from proc filesystem file or directory.
 * Receives file struct pointer parameter "f", string parameter "addr", int
 * parameter "n". "f" is a pointer to the file we read from. "addr" is a pointer
 * to the string we read the contents into. "n" is the amount of characters to
 * read. Return values: -1 on failure. amount of characters read on success.
 * currently supports reading from supported unsafe_proc_open files.
 */
int unsafe_proc_read(struct vfs_file* f, char* addr, int n);

/**
 * This function write to proc filesystem file.
 * Receives file struct pointer parameter "f", buffer parameter "addr", int
 * parameter "n". "f" is a pointer to the file we write to. "addr" is a pointer
 * to the buffer we write the contents from. "n" is the amount of characters to
 * write. Return values: -1 on failure. amount of characters write on success.
 * currently supports writing to supported unsafe_proc_open files.
 */
int unsafe_proc_write(struct vfs_file* f, char* addr, int n);

/**
 * This function gets stats of proc file or directory.
 * Receives file struct pointer parameter "f", stat struct pointer parameter
 * "st". "f" is a pointer to the we get the stats of. "st" is a pointer struct
 * we write the stats to. Return values: -1 on failure. 0 on success.
 */
int unsafe_proc_stat(struct vfs_file* f, struct stat* st);

/* No need of proc_close function due to use of casual reference counter
 * (f->ref). */

#endif /* XV6_PROCFS_H */
