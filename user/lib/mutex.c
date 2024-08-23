#include "mutex.h"

#include "../../common/fcntl.h"
#include "../../common/stat.h"
#include "user.h"

/**
 * This method initializes the mutex directory, if it doesn't exist,
 * on first call - to enable the mutex to work.
 */
static enum mutex_e create_mutex_directory() {
  struct stat mutex_stat;
  // Create /mutex/ directory if it doesn't exist.
  if (stat(MUTEX_PREFIX, &mutex_stat) < 0) {
    if (mkdir(MUTEX_PREFIX) < 0) {
      MUTEX_LOG_DEBUG("Mutex path doesn't exist and can't be created\n");
      return MUTEX_DIR_INIT_FAILED;
    }
  } else {
    if (mutex_stat.type != T_DIR) {
      MUTEX_LOG_DEBUG("Mutex path exists and is not a directory\n");
      return MUTEX_DIR_INIT_FAILED;
    }
  }
  return MUTEX_SUCCESS;
}

enum mutex_e mutex_init_named(mutex_t *const mutex_var,
                              const char *const name) {
  char *buffer = NULL;
  uint name_len = 0;

  if (NULL == mutex_var || name == NULL) return MUTEX_INVALID_PARAMETER;

  name_len = strlen(name);
  if (name_len > MAX_INT_ASCII_DIGITS || name_len < 1)
    return MUTEX_INVALID_PARAMETER;

  enum mutex_e res;
  if ((res = create_mutex_directory()) != MUTEX_SUCCESS) return res;

  buffer = mutex_var->buffer;

  *buffer = '\0';
  strcat(buffer, MUTEX_PREFIX);
  strcat(buffer, name);

  MUTEX_LOG_DEBUG("init mutex with path: %s\n", buffer);

  return MUTEX_SUCCESS;
}

/* Init a unique mutex. */
enum mutex_e mutex_init(mutex_t *mutex_var) {
  int timeid;

  // Using uptime() to have a unique mutex filename as
  //    xv6 doesn't support nanoseconds resolution.
  if ((timeid = uptime()) < 0) {
    return MUTEX_UPTIME_ERROR;
  }

  // Timestamp to string
  char timeid_str[MAX_INT_ASCII_DIGITS + 1];
  itoa(timeid_str, timeid);

  int res = mutex_init_named(mutex_var, timeid_str);
  if (res != MUTEX_SUCCESS) return res;

  sleep(1);  // Skip tick - otherwise, we got the same tick in tests.

  return res;
}

/* Locks a mutex if unlocked, sleep otherwise - res might indicates an error. */
enum mutex_e mutex_lock(mutex_t *mutex_var) {
  int res = MUTEX_FAILURE;

  if (NULL == mutex_var) return MUTEX_INVALID_PARAMETER;

  // pathname already exists and O_CREAT and O_EXCL were used
  while ((res = open(mutex_var->buffer, O_CREATE | O_EXCL)) == EEXIST) {
    MUTEX_LOG_DEBUG("Mutes %s is locked....\n", mutex_var->buffer);
    // Sleep to make it less busy
    sleep(MUTEX_RETRY_MS);
  }

  MUTEX_LOG_DEBUG("Mutes %s is %d unlocked....\n", mutex_var->buffer, res);

  if (res < 0) {
    MUTEX_LOG_DEBUG("Mutex lock failed\n");
    return res;
  }
  return MUTEX_SUCCESS;
}

/* Unlocks a mutex - returns according to unlink return values */
enum mutex_e mutex_unlock(mutex_t *mutex_var) {
  if (NULL == mutex_var) return MUTEX_INVALID_PARAMETER;

  if (unlink(mutex_var->buffer) < 0) {
    MUTEX_LOG_DEBUG("Mutex unlock failed\n");
    return MUTEX_FAILURE;
  }
  return MUTEX_SUCCESS;
}
