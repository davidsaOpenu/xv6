// User-created containers interface functions.
// The public functions in this file are called directly by the CLI.

#include "start.h"

#include "configs.h"
#include "container.h"
#include "lib/mutex.h"
#include "util.h"

static const char* container_exec_path = "sh";
static const char* start_container_argv[] = {"sh", 0};

/**
 * Mounts configuration for pouch start container command.
 */
static const struct container_mounts_def pouch_start_mounts[] = {
    // Mount root filesystem first, and umount it last.
    {
        .src = NULL,
        .dest = "",
        .type = IMAGE_ROOT_FS,
    },
    // Pouch configuration & dev directories are required to allow pouch to
    // work.
    {
        .src = DEV_DIR,
        .dest = DEV_DIR,
        .type = BIND_MOUNT,
    },
    {
        .src = POUCH_CONFIGS_DIR,
        .dest = POUCH_CONFIGS_DIR,
        .type = BIND_MOUNT,
    },
    // Mutex directory is needed for mutexes to work across containers.
    {
        .src = MUTEX_PREFIX,
        .dest = MUTEX_PREFIX,
        .type = BIND_MOUNT,
    },
    {.type = LIST_END}};

pouch_status pouch_start_child_func(
    const struct container_start_config* config) {
  // Execute the shell in the container.
  // No return unless there is an error in exec.
  exec(container_exec_path, start_container_argv);
  return ERROR_CODE;
}

pouch_status pouch_do_container_start(const char* container_name,
                                      const char* const image_name) {
  struct container_start_config info = {.child_func = pouch_start_child_func,
                                        .mounts = pouch_start_mounts,
                                        .daemonize = XV_TRUE};
  strcpy(info.image_name, image_name);
  strcpy(info.container_name, container_name);
  return _pouch_container_start(&info);
}

pouch_status pouch_do_container_stop(const char* const container_name) {
  mutex_t global_pouch_mutex = {0};
  container_config conf;
  pouch_status ret = SUCCESS_CODE;

  if (init_and_lock_pouch_global_mutex(&global_pouch_mutex) != SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to lock global mutex\n");
    return POUCH_MUTEX_ERROR_CODE;
  }

  if ((ret = pouch_cconf_read(container_name, &conf)) != SUCCESS_CODE) {
    printf(stdout, "Pouch: %s container not found\n", container_name);
    goto end;
  }

  // Make sure this is a daemonized container!
  if (conf.tty_num < 0) {
    printf(stderr, "Pouch: %s is not a pouch started container\n",
           container_name);
    ret = CONTAINER_NOT_FOUND_CODE;
    goto end;
  }

  if ((ret = _pouch_container_stop(&conf)) != SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to stop container %s\n", container_name);
    goto end;
  }

end:
  if (mutex_unlock(&global_pouch_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock global mutex");
    ret = POUCH_MUTEX_ERROR_CODE;
  }
  return ret;
}
