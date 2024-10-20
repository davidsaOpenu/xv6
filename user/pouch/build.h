#ifndef XV6_USER_POUCH_BUILD_H
#define XV6_USER_POUCH_BUILD_H

#include "pouch.h"

#define POUCHFILE_IMPORT_TOKEN "IMPORT"
#define POUCHFILE_RUN_TOKEN "RUN"
#define POUCHFILE_COPY_TOKEN "COPY"

enum pouchfile_command_type {
  COMMAND_NONE = 0,
  POUCHFILE_IMPORT,
  POUCHFILE_RUN,
  POUCHFILE_COPY,
};

struct command_to_type {
  const char* const command;
  enum pouchfile_command_type type;
};

extern const struct command_to_type command_to_type_map[];

/**
 * defines a single command in a pouchfile.
 */
struct pouchfile_command {
  enum pouchfile_command_type type;
  union {
    char* command;        // RUN
    char* image_name;     // IMPORT
    char* files_to_copy;  // CP
  };
  struct pouchfile_command* next;
};

/**
 * defines a parsed pouchfile to be built.
 */
struct pouchfile {
  struct pouchfile_command* commands_list_head;
  int ncmds;
};

/**
 * Builds a pouch image from a given file name to a specified tag.
 */
pouch_status pouch_build(const char* file_name, const char* tag);

#endif  // XV6_USER_POUCH_BUILD_H
