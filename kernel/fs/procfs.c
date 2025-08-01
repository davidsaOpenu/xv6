#include "procfs.h"

#include "defs.h"
#include "device/buf_cache.h"
#include "device/device.h"
#include "fcntl.h"
#include "kalloc.h"
#include "mount_ns.h"
#include "namespace.h"
#include "param.h"
#include "vfs_file.h"

// Static to save space in the stack.
static char buf[MAX_BUF];

static int fdalloc(struct vfs_file* f) {
  int fd;
  struct proc* curproc = myproc();

  for (fd = 0; fd < NOFILE; fd++) {
    if (curproc->ofile[fd] == 0) {
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

/**
 * This function copies from a given string into a given buffer based on the
 * input parameters.
 *
 * Receives char parameter pointer to pointer "buffer", char parameter pointer
 * "string".
 *
 * The function copies from "string" into "buffer" "size" amount of characters
 * up to "MAX_PROC_FILE_NAME_LENGTH". In addition "buffer" index is advanced by
 * "MAX_PROC_FILE_NAME_LENGTH".
 */
static inline void copy_and_move_buffer_max_len(char** buffer, char* str) {
  int len = min(strlen(str), MAX_PROC_FILE_NAME_LENGTH);
  strncpy(*buffer, str, len);
  *buffer += MAX_PROC_FILE_NAME_LENGTH;
}

/**
 * This function copies from a given string into a given buffer based on the
 * input parameters upto size.
 *
 * Receives char parameter pointer to pointer "buffer", char parameter pointer
 * "string", int parameter "size".
 *
 * In addition "buffer" index is advanced by min between string length or size.
 * The function returns the amount of bytes copied to the dest buffer.
 */
static inline void copy_and_move_buffer(char** buffer, char* str, int size) {
  int len = min(strlen(str), size);  // NOLINT(build/include_what_you_use)
  strncpy(*buffer, str, len);
  *buffer += len;
}

/**
 * This function copies from given buffer into a given address based on the
 * input parameters. The function gets stopped when it encounters a null
 * terminator character in the input buffer, and instead, replace it with a
 * newline character. Receives char parameter pointer "src", int parameter
 * "size", char parameter pointer "dest".
 *
 * "src" is the source we are reading from.
 * "size" is an integer indicating how many characters we allowed to copy.
 * "dest" is the buffer which "buf" is being copied into.
 *
 * The function returns the amount of bytes copied to the dest buffer.
 */
static inline int copy_buffer_with_newline(char* dest, char* src, int size) {
  int i;
  for (i = 0; i < size; i++) {
    if (!src[i]) {
      dest[i] = '\n';
      break;
    }
    dest[i] = src[i];
  }
  return i;
}

/**
 * Copies buf to dest from offset till strlen of buf or max bytes reached.
 *
 * "dest" - Buffer to copy to.
 * "off" - Offset to start copy from.
 * "n" - Max bytes to copy.
 *
 * Returns bytes copied.
 */
static inline int copy_buffer(char* dest, int off, int n) {
  int i = 0;
  int buf_len = strlen(buf);
  for (; i < (buf_len - off) && i < n; i++) dest[i] = buf[i + off];

  return i;
}

static proc_file_name_t get_file_name_constant(char* filename) {
  if (strcmp(filename, PROCFS_MEM) == 0) return PROC_MEM;

  if (strcmp(filename, PROCFS_MOUNTS) == 0) return PROC_MOUNTS;

  if (strcmp(filename, PROCFS_DEVICES) == 0) return PROC_DEVICES;

  if (strcmp(filename, PROCFS_CACHE) == 0) return PROC_CACHE;

  if (strcmp(filename, PROCFS_KMEMTEST) == 0) return PROC_KMEMTEST;

  return NONE;
}

int unsafe_proc_open_file(char* filename, int omode) {
  int fd = -1;
  struct vfs_file* f;
  uint file_writeable = 0;
  struct mount_list* entry;
  proc_file_name_t filename_const = get_file_name_constant(filename);

  if (NONE == filename_const) return -1;

  /* Allocate file structure and file descriptor. */
  if ((f = vfs_filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
    if (f) vfs_fileclose(f);
    return -1;
  }

  switch (filename_const) {
    case PROC_MEM:
      f->proc.mem = myproc()->sz;
      break;

    case PROC_MOUNTS:
      acquire(&(myproc()->nsproxy->mount_ns->lock));
      f->proc.mount_entry = myproc()->nsproxy->mount_ns->active_mounts;
      /* Count entries. */
      entry = f->proc.mount_entry;
      while (entry != 0) {
        f->count++;
        entry = entry->next;
      }
      release(&(myproc()->nsproxy->mount_ns->lock));
      break;

    case PROC_DEVICES:
      memset(f->proc.devs, 0, sizeof(f->proc.devs));
      acquire(&dev_holder.lock);
      f->count = 0;
      for (int i = 0; i < NMAXDEVS; i++) {
        if (dev_holder.devs[i].type != DEVICE_TYPE_LOOP) continue;
        f->proc.devs[i].private = dev_holder.devs[i].private;
        f->proc.devs[i].ref = dev_holder.devs[i].ref;
        f->proc.devs[i].type = dev_holder.devs[i].type;
        if (dev_holder.devs[i].ref != 0) f->count++;
      }
      release(&dev_holder.lock);
      break;

    case PROC_CACHE:
      file_writeable = 1;
      break;

    case PROC_KMEMTEST:
      break;

    default:
      break;
  }

  /* General file. */
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = file_writeable && ((omode & O_WRONLY) || (omode & O_RDWR));
  f->ref++;  // That way don't need to take care of closing.

  /* FD_PROC file. */
  f->type = FD_PROC;
  f->filetype = T_PROCFILE;
  f->filename_const = filename_const;
  strncpy(f->filename, filename, sizeof(f->filename));

  return fd;
}

int unsafe_proc_open_dir(char* filename, int omode) {
  struct vfs_file* f;
  int fd = -1;

  if (*procfs_root == 0) return -1;

  /* Allocate file structure and file descriptor. */
  if ((f = vfs_filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
    if (f) vfs_fileclose(f);
    return -1;
  }

  /* General dir. */
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = NON_WRITABLE;
  f->ref++;  // That way don't need to take care of closing.

  /* FD_PROC dir. */
  f->type = FD_PROC;
  f->filetype = T_PROCDIR;
  *f->filename = 0;

  return fd;
}

int unsafe_proc_open(int filetype, char* filename, int omode) {
  int fd = -1;

  switch (filetype) {
    case T_PROCFILE:
      fd = unsafe_proc_open_file(filename, omode);
      break;

    case T_PROCDIR:
      fd = unsafe_proc_open_dir(filename, omode);
      break;

    default:
      break;
  }

  return fd;
}

static int read_file_proc_mem(struct vfs_file* f, char* addr, int n) {
  int i = 0;
  char* mem_text = buf;

  uint text_size = utoa(mem_text, f->proc.mem);
  for (; i < (text_size - f->off) && i < n; i++) addr[i] = mem_text[f->off + i];

  return i;
}

static int read_file_proc_mounts(struct vfs_file* f, char* addr, int n) {
  struct mount_list* entry = f->proc.mount_entry;
  char* bufp = buf;
  uint mindex = 0;

  memset(bufp, 0, sizeof(buf));

  copy_buffer_with_newline(bufp, MOUNTS_TITLE, sizeof(MOUNTS_TITLE));
  while (entry != 0) {
    mindex++;
    bufp += utoa(bufp, mindex);

    copy_and_move_buffer(&bufp, MOUNTS_MOUNT, sizeof(MOUNTS_MOUNT));
    bufp += utoa(bufp, (uint)&entry->mnt);

    copy_and_move_buffer(&bufp, MOUNTS_ATTACHED_TO, sizeof(MOUNTS_ATTACHED_TO));
    bufp += utoa(bufp, (uint)entry->mnt.mountpoint);

    copy_and_move_buffer(&bufp, MOUNTS_CHILD_OF, sizeof(MOUNTS_CHILD_OF));
    bufp += utoa(bufp, (uint)entry->mnt.parent);

    copy_and_move_buffer(&bufp, MOUNTS_WITH_REF, sizeof(MOUNTS_WITH_REF));
    bufp += itoa(bufp, entry->mnt.ref);

    *bufp++ = '\n';
    entry = entry->next;
  }

  return copy_buffer(addr, f->off, n);
}

static int read_file_proc_devices(struct vfs_file* f, char* addr, int n) {
  char* bufp = buf;
  uint mindex = 0;

  memset(buf, 0, sizeof(buf));

  for (int i = 0; i < NMAXDEVS; i++) {
    if (f->proc.devs[i].ref == 0 || f->proc.devs[i].type != DEVICE_TYPE_LOOP)
      continue;

    copy_and_move_buffer(&bufp, DEVICES_DEVICE, sizeof(DEVICES_DEVICE));

    mindex++;
    bufp += utoa(bufp, mindex);

    copy_and_move_buffer(&bufp, DEVICES_BACKED_BY_INODE,
                         sizeof(DEVICES_BACKED_BY_INODE));
    bufp += utoa(bufp, (uint)f->proc.devs[i].private);

    copy_and_move_buffer(&bufp, DEVICES_WITH_REF, sizeof(DEVICES_WITH_REF));
    bufp += itoa(bufp, f->proc.devs[i].ref);

    *bufp++ = '\n';
  }

  return copy_buffer(addr, f->off, n);
}

static int read_file_proc_cache(struct vfs_file* f, char* addr, int n) {
  if (buf_cache_is_cache_enabled()) {
    strncpy(buf, CACHE_ENABLED, sizeof(CACHE_ENABLED));
  } else {
    strncpy(buf, CACHE_DISABLED, sizeof(CACHE_DISABLED));
  }

  return copy_buffer(addr, f->off, n);
}

static int write_file_proc_cache(struct vfs_file* f, char* addr, int n) {
  if ((n == (sizeof(CACHE_ENABLED) - 1)) &&
      (0 == memcmp(addr, CACHE_ENABLED, n))) {
    buf_cache_enable_cache();
    return sizeof(CACHE_ENABLED) - 1;
  } else if ((n == (sizeof(CACHE_DISABLED) - 1)) &&
             (0 == memcmp(addr, CACHE_DISABLED, n))) {
    buf_cache_disable_cache();
    return sizeof(CACHE_DISABLED) - 1;
  }

  return RESULT_ERROR;
}

static int read_file_proc_kmemtest(struct vfs_file* f, char* addr, int n) {
  char* bufp = buf;
  memset(buf, 0, sizeof(buf));

  kmemtest_info info;
  (void)kmemtest(&info);

  copy_and_move_buffer(&bufp, KMEMTEST_TITLE, sizeof(KMEMTEST_TITLE));
  *bufp++ = '\n';
  copy_and_move_buffer(&bufp, KMEMTEST_COUNTER, sizeof(KMEMTEST_COUNTER));
  bufp += utoa(bufp, (uint)info.page_cnt);
  *bufp++ = '\n';
  copy_and_move_buffer(&bufp, KMEMTEST_LIST, sizeof(KMEMTEST_LIST));
  bufp += utoa(bufp, (uint)info.list_cnt);
  *bufp++ = '\n';
  copy_and_move_buffer(&bufp, KMEMTEST_ERRORS, sizeof(KMEMTEST_ERRORS));
  bufp += utoa(bufp, (uint)info.err_cnt);
  *bufp++ = '\n';
  return copy_buffer(addr, f->off, n);
}

int unsafe_proc_read(struct vfs_file* f, char* addr, int n) {
  int result = RESULT_ERROR;
  char* bufp = buf;

  if (f->readable == 0) return RESULT_ERROR;

  if (f->filetype == T_PROCFILE) {
    /* Read proc file. */
    switch (f->filename_const) {
      case PROC_MEM:
        result = read_file_proc_mem(f, addr, n);
        break;

      case PROC_MOUNTS:
        result = read_file_proc_mounts(f, addr, n);
        break;

      case PROC_DEVICES:
        result = read_file_proc_devices(f, addr, n);
        break;

      case PROC_CACHE:
        result = read_file_proc_cache(f, addr, n);
        break;

      case PROC_KMEMTEST:
        result = read_file_proc_kmemtest(f, addr, n);
        break;

      default:
        return RESULT_ERROR;
    }
  } else if (f->filetype == T_PROCDIR) {
    /* Read proc directory. */
    if (*f->filename == 0) {
      /* /proc dir. */
      memset(buf, ' ', sizeof(buf));

      copy_and_move_buffer_max_len(&bufp, ".");
      copy_and_move_buffer_max_len(&bufp, PROCFS_MEM);
      copy_and_move_buffer_max_len(&bufp, PROCFS_MOUNTS);
      copy_and_move_buffer_max_len(&bufp, PROCFS_DEVICES);
      copy_and_move_buffer_max_len(&bufp, PROCFS_CACHE);
      copy_and_move_buffer_max_len(&bufp, PROCFS_KMEMTEST);

      *bufp++ = '\0';

      result = copy_buffer(addr, f->off, n);
    } else {
      return RESULT_ERROR;
    }
  }

  // All the read_file functions doesn't return an error.
  f->off += result;

  return result;
}

int unsafe_proc_write(struct vfs_file* f, char* addr, int n) {
  int result = RESULT_ERROR;

  if (f->writable == 0) return RESULT_ERROR;

  // Allow writing only to regular proc files, not the /proc dir itself.
  if (f->filetype == T_PROCFILE) {
    /* Read proc file. */
    switch (f->filename_const) {
      case PROC_CACHE:
        result = write_file_proc_cache(f, addr, n);
        break;

      default:
        return RESULT_ERROR;
    }
  } else {
    return RESULT_ERROR;
  }

  return result;
}

static int proc_file_size(struct vfs_file* f) {
  int size = 0;
  struct mount_list* entry;

  switch (f->filename_const) {
    case PROC_MEM:
      size = sizeof(f->proc.mem);
      break;

    case PROC_MOUNTS:
      entry = f->proc.mount_entry;
      size += sizeof(uint);  // index.
      size += sizeof(MOUNTS_MOUNT);
      size += sizeof(&entry->mnt);
      size += sizeof(MOUNTS_ATTACHED_TO);
      size += sizeof(entry->mnt.mountpoint);
      size += sizeof(MOUNTS_CHILD_OF);
      size += sizeof(entry->mnt.parent);
      size += sizeof(MOUNTS_WITH_REF);
      size += sizeof(entry->mnt.ref);
      size += 1;  // \n.
      size *= f->count;
      break;

    case PROC_DEVICES:
      size += sizeof(DEVICES_DEVICE);
      size += sizeof(uint);  // index.
      size += sizeof(DEVICES_BACKED_BY_INODE);
      size += sizeof(f->proc.devs[0].private);
      size += sizeof(DEVICES_WITH_REF);
      size += sizeof(f->proc.devs[0].ref);
      size += 1;  // \n.
      size *= f->count;
      break;

    case PROC_CACHE:
      size = CACHE_STATUS_LEN;
      break;

    case PROC_KMEMTEST:
      size += sizeof(KMEMTEST_TITLE) + 1;  // \n.
      size += sizeof(KMEMTEST_COUNTER) + sizeof(((kmemtest_info*)0)->page_cnt) +
              1;  // \n.
      size += sizeof(KMEMTEST_LIST) + sizeof(((kmemtest_info*)0)->list_cnt) +
              1;  // \n.
      size += sizeof(KMEMTEST_ERRORS) + sizeof(((kmemtest_info*)0)->err_cnt) +
              1;  // \n.
      break;

    default:
      break;
  }

  return size;
}

int unsafe_proc_stat(struct vfs_file* f, struct stat* st) {
  if (f->filetype == T_PROCDIR && *f->filename == 0) {
    /* /proc dir. */
    st->type = T_PROCDIR;
    st->size = MAX_PROC_FILE_NAME_LENGTH * PROC_FILE_NAME_END;
  } else if (f->filetype == T_PROCFILE) {
    st->type = T_PROCFILE;
    st->size = proc_file_size(f);
  } else {
    return RESULT_ERROR;
  }

  return RESULT_SUCCESS;
}
