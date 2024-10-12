#ifndef USER_POUCH_UTIL_H
#define USER_POUCH_UTIL_H

#include "pouch.h"

/**
 * Create a directory if it does not exist.
 * If the path exists, makes sure it is a directory indeed.
 */
pouch_status mkdir_if_not_exist(const char* path);

#endif  // USER_POUCH_UTIL_H
