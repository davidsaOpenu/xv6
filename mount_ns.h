#ifndef XV6_MOUNT_NS_H
#define XV6_MOUNT_NS_H

#include "mount.h"
#include "param.h"
#include "spinlock.h"

struct mount_list {
  struct mount mnt;
  struct mount_list* next;
};

/* A bind table, acts as a linked list where each node is a pair
 * of a source path and a target path,
 * refrenced when resolving paths to redirect access from source to target
 * A bind table belongs to a mount_ns, so that the entire mount namespace
 * would share the same view of the file system
 */
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
  struct bind_mount_list* bind_table;  // Table of binds shared across all
                                       // processes inside the mount namespace
};

#endif /* XV6_MOUNT_NS_H */
