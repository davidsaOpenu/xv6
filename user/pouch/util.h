#ifndef USER_POUCH_UTIL_H
#define USER_POUCH_UTIL_H

#include "lib/mutex.h"
#include "pouch.h"

#define POUCH_DEBUG

#ifdef POUCH_DEBUG
#define POUCH_LOG_DEBUG(msg, ...)                      \
  do {                                                 \
    printf(stdout, "POUCH:DEBUG:" msg, ##__VA_ARGS__); \
  } while (0)
#else
#define POUCH_LOG_DEBUG(msg, ...)
#endif

/**
 * Initialize and lock the pouch global mutex.
 */
pouch_status init_and_lock_pouch_global_mutex(mutex_t* const mutex);

/**
 * Create a directory if it does not exist.
 * If the path exists, makes sure it is a directory indeed.
 */
pouch_status mkdir_if_not_exist(const char* path);

/**
 * Copy a file from source to target.
 */
pouch_status cp(const char* src, const char* target);

#endif  // USER_POUCH_UTIL_H
