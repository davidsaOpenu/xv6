#include "util.h"

#include "lib/user.h"
#include "stat.h"

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
