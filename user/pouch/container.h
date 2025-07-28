#ifndef XV6_USER_POUCH_CONTAINER_H
#define XV6_USER_POUCH_CONTAINER_H

#include "configs.h"
#include "param.h"
#include "pouch.h"
#include "types.h"

enum container_mount_type { IMAGE_ROOT_FS = 1, BIND_MOUNT, LIST_END };

/**
 * Defines a mount to be used in a container.
 */
struct container_mounts_def {
  const char* src;
  const char* dest;
  enum container_mount_type type;
};

/**
 * Defines properties to start a container.
 */
struct container_start_config {
  char image_name[MAX_PATH_LENGTH];
  char container_name[CNTNAMESIZE];

  /** Specifies the type of container to be started. */
  XV_Bool daemonize;

  /** Specifies mounts to apply when starting the container. */
  const struct container_mounts_def* mounts;

  /** Called in the context of the container's child process after it has
   * started. */
  pouch_status (*child_func)(const struct container_start_config* config);

  /**
   * Data to be used by different users of the container start,
   * to be passed from the caller to child and parent functions.
   */
  void* private_data;

  /** The mount point of the container image.
   * This is filled in by the container start function.
   */
  char image_mount_point[MAX_PATH_LENGTH];
};

/**
 * Returns XV_TRUE if the current process is running in a container.
 */
XV_Bool pouch_container_is_attached();

/*
 *   Pouch start (internal):
 *   - Starting new container by the given start configuration.
 * exit
 *   @input: config
 */
pouch_status _pouch_container_start(struct container_start_config* config);

/*
 *   Pouch stop (internal):
 *   - Stopping container internally.
 *  pouch global lock must be held before calling this function.
 *   @input: container_config - container configuration to stop
 */
pouch_status _pouch_container_stop(const container_config* const conf);

/*
 *  Pouch connect:
 *   - Connect to container
 *   @input: container_name - container name to connect
 */
pouch_status pouch_container_connect(const char* const container_name);

/*
 *   Pouch disconnect:
 *   - Disconnect from container
 */
pouch_status pouch_container_disconnect();

/*
 *   Print given container information
 *   - show all started containers and their state
 *   @input: container_name - container name to print, if NULL print connected
 * container.
 */
pouch_status pouch_container_print_info(const char* const container_name);

/*
 *   Get connected container name
 *   @output: cname - connected container name
 */
pouch_status pouch_container_get_connected_name(char* const cname);

/*
 *   Limit pouch cgroup:
 *   - Limits given state object for given container name and limit
 *   @input: container_name, cgroup_state_obj, limitation
 */
pouch_status pouch_container_limit(const char* const container_name,
                                   const char* const cgroup_state_obj,
                                   const char* const limitation);

/*
 *   Print cotainers list
 *   - show all started containers and their state
 *   @output: none
 */
pouch_status pouch_containers_print_all();

#endif  // XV6_USER_POUCH_CONTAINER_H
