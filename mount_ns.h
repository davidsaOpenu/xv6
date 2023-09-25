#ifndef XV6_MOUNT_NS_H
#define XV6_MOUNT_NS_H

#include "mount.h"
#include "spinlock.h"
#include "param.h"

struct mount_list {
  struct mount mnt;
  struct mount_list* next;
};

struct bind_mount_list {
  char source[MAX_PATH_LENGTH];
  char target[MAX_PATH_LENGTH];
  struct bind_mount_list* next;
};

/* Note the structure of active_mounts. New mounts are always added to the
 * front, so the mount "parent" referencesare always pointing to entries
 * farther in the back.
 */

struct mount_ns {
  int ref;
  struct spinlock lock;  // protects active_mounts
  struct mount* root;
  struct mount_list* active_mounts;
  struct bind_mount_list* bind_table;
};

#endif /* XV6_MOUNT_NS_H */
