#include "mutex.h"

#include "fcntl.h"
#include "user.h"

#define MUTEX_RETRY_MS (10)

int mutex_init_named(mutex_t *const mutex_var, const char *const name) {
  char *buffer = NULL;
  uint name_len = 0;

  if (NULL == mutex_var || name == NULL) return MUTEX_INVALID_PARAMETER;

  name_len = strlen(name);
  if (name_len > MAX_INT_ASCII_DIGITS || name_len < 1)
    return MUTEX_INVALID_PARAMETER;

  buffer = mutex_var->buffer;

  *buffer = '\0';
  strcat(buffer, MUTEX_PREFIX);
  strcat(buffer, name);

  return MUTEX_SUCCESS;
}

/* Init a unique mutex. */
int mutex_init(mutex_t *mutex_var) {
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
int mutex_lock(mutex_t *mutex_var) {
  int res = MUTEX_FAILURE;

  if (NULL == mutex_var) return MUTEX_INVALID_PARAMETER;

  // pathname already exists and O_CREAT and O_EXCL were used
  while ((res = open(mutex_var->buffer, O_CREATE | O_EXCL)) == EEXIST) {
    // Sleep to make it less busy
    sleep(MUTEX_RETRY_MS);
  }

  return res;
}

/* Unlocks a mutex - returns according to unlink return values */
int mutex_unlock(mutex_t *mutex_var) {
  if (NULL == mutex_var) return MUTEX_INVALID_PARAMETER;

  return unlink(mutex_var->buffer);
}
