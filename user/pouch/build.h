#ifndef XV6_USER_POUCH_BUILD_H
#define XV6_USER_POUCH_BUILD_H

#include "pouch.h"

#define POUCHFILE_IMPORT_TOKEN "IMPORT"
#define POUCHFILE_RUN_TOKEN "RUN"

/**
 * defines a single command in a pouchfile.
 */
struct pouchfile_command {
  char* command;
  struct pouchfile_command* next;
};

/**
 * defines a parsed pouchfile to be built.
 */
struct pouchfile {
  char* image_name;
  struct pouchfile_command* commands_list_head;
};

/**
 * Builds a pouch image from a given file name to a specified tag.
 */
pouch_status pouch_image_build(const char* file_name, const char* tag);

#endif  // XV6_USER_POUCH_BUILD_H
