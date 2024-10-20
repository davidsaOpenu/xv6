// General utility functions for pouch

#include "util.h"

#include "fcntl.h"
#include "stat.h"

#define CONTAINERS_GLOBAL_LOCK_NAME "cnts_gllk"

pouch_status init_and_lock_pouch_global_mutex(mutex_t* const mutex) {
  enum mutex_e res = mutex_init_named(mutex, CONTAINERS_GLOBAL_LOCK_NAME);
  if (res != MUTEX_SUCCESS) {
    printf(stderr, "Failed to init pouch global mutex\n");
    return ERROR_CODE;
  }
  if (mutex_lock(mutex) != MUTEX_SUCCESS) {
    printf(stderr, "Failed to lock pouch global mutex\n");
    return ERROR_CODE;
  }
  return SUCCESS_CODE;
}

pouch_status mkdir_if_not_exist(const char* path) {
  struct stat st;
  if (stat(path, &st) < 0) {
    if (mkdir(path) < 0) {
      printf(stderr, "Cannot create directory %s\n", path);
      return ERROR_CODE;
    }
    if (stat(path, &st) < 0) {
      printf(stderr, "Cannot stat directory %s\n", path);
      return ERROR_CODE;
    }
  }
  if (st.type != T_DIR && st.type != T_PROCDIR && st.type != T_CGDIR) {
    printf(stderr, "Path %s exist but is not a directory (%d)\n", path,
           st.type);
    return ERROR_CODE;
  }
  return SUCCESS_CODE;
}

pouch_status cp(const char* src, const char* target) {
  int fd = 0, target_fd = 0;
  struct stat st;
  int ret = ERROR_CODE;

  if ((fd = open(src, O_RDONLY)) < 0) {
    printf(stderr, "can't locate %s\n", src);
    ret = ERROR_CODE;
    goto end;
  }

  if (fstat(fd, &st) < 0) {
    printf(stderr, "cannot stat %s\n", src);
    ret = ERROR_CODE;
    goto end;
  }

  if (st.type == T_DIR || st.type == T_CGDIR || st.type == T_PROCDIR) {
    printf(stderr, "cannot copy directory %s\n", src);
    ret = ERROR_CODE;
    goto end;
  }

  target_fd = open(target, O_CREATE | O_RDWR);
  if (target_fd < 0) {
    printf(stderr, "cannot create %s\n", target);
    ret = ERROR_CODE;
    goto end;
  }

  char buf[512];
  int n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(target_fd, buf, n) != n) {
      printf(stderr, "write error copying %s to %s\n", src, target);
      ret = ERROR_CODE;
      goto end;
    }
  }

  if (n < 0) {
    printf(stderr, "read error copying %s to %s\n", src, target);
    ret = ERROR_CODE;
    goto end;
  }

  ret = SUCCESS_CODE;

end:
  if (fd > 0) close(fd);
  if (target_fd > 0) close(target_fd);
  return ret;
}
