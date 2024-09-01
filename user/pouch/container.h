#include "pouch.h"
#include "types.h"

/*
 *   Container name maximum size
 */
#define CNTNAMESIZE (100)

/**
 * Returns true if the current process is running in a container.
 */
bool pouch_container_is_attached();

/*
 *   Pouch fork:
 *   - Starting new container and execute shell inside, waiting for container to
 * exit
 *   @input: container_name,root_dir
 */
pouch_status pouch_container_start(const char* const container_name,
                                   const char* const image_name);

/*
 *   Pouch stop:
 *   - Stopping container
 *   @input: container_name - container name to stop
 */
pouch_status pouch_container_stop(const char* const container_name);

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
