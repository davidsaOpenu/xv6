// Pouch container management functions.

#include "container.h"

#include "fcntl.h"
#include "image.h"
#include "lib/mutex.h"
#include "ns_types.h"
#include "stat.h"
#include "util.h"
#include "wstatus.h"

#define IMAGE_MOUNT_DIR "/mnt/"
/** When pivoting root during container startup, this is the location of the
 * already pivoted, old root. */
static const char old_root_relative_path[] = "/.old_root";

bool pouch_container_is_attached() { return getppid() == 1; }

/*
 *   Prepate cgroup name:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: container_name
 *   @output: cgcname
 */
static pouch_status prepare_cgroup_cname(const char* const container_name,
                                         char* const cg_cname) {
  if (strlen(container_name) + strlen(POUCH_CGROUPS_DIR) + 1 >
      MAX_PATH_LENGTH) {
    perror("Container name is too long");
    return CONTAINER_NAME_TOO_LONG_CODE;
  }
  strcpy(cg_cname, POUCH_CGROUPS_DIR);
  strcat(cg_cname, "/");
  strcat(cg_cname, container_name);
  return SUCCESS_CODE;
}

/*
 *   Create pouch cgroup:
 *   - Creates cgroup for a container inside root cgroup
 *   - Enables cpu.controller
 *   @input: cg_cname - cgroups fs path to a new cgroup
 *   @output: none
 */
static pouch_status create_pouch_cgroup(const char* const cg_cname,
                                        const char* const cname) {
  if (mkdir(cg_cname) != 0) {
    printf(stderr,
           "Pouch: Failed to create cgroup with the given name. Consider "
           "another container name: %s \n",
           cname);
    return CGROUP_CREATE_FAILED_CODE;
  }
  char cgpath[MAX_PATH_LENGTH];
  memset(cgpath, '\0', sizeof(cgpath));
  strcpy(cgpath, cg_cname);
  strcat(cgpath, "/cgroup.subtree_control");

  int cgroup_subtree_control_fd = open(cgpath, O_RDWR);

  if (cgroup_subtree_control_fd < 0) return -1;

  // Enable cpu controller
  char buf[256];
  memset(buf, '\0', sizeof(buf));
  strcpy(buf, "+cpu");
  if (write(cgroup_subtree_control_fd, buf, sizeof(buf)) < 0) return -1;
  if (close(cgroup_subtree_control_fd) < 0) return -1;
  return SUCCESS_CODE;
}

pouch_status pouch_container_limit(const char* const container_name,
                                   const char* const cgroup_state_obj,
                                   const char* const limitation) {
  char cg_limit_cname[MAX_PATH_LENGTH];

  prepare_cgroup_cname(container_name, cg_limit_cname);
  strcat(cg_limit_cname, "/");
  strcat(cg_limit_cname, cgroup_state_obj);

  if (pouch_cconf_read(container_name, NULL) != SUCCESS_CODE) {
    printf(stderr, "Pouch: %s container not found\n", container_name);
    goto error;
  }

  int cname_fd = open(cg_limit_cname, O_RDWR);
  if (cname_fd < 0) {
    printf(stderr, "Incorrect cgroup object-state provided. Not applied.\n",
           container_name);
    goto error;
  }

  if (write(cname_fd, limitation, sizeof(limitation)) < 0) {
    printf(stderr, "Cannot write to %s\n", cg_limit_cname);
    close(cname_fd);
    goto error;
  }
  if (close(cname_fd) < 0) {
    printf(stderr, "Cannot close %s\n", cg_limit_cname);
    goto error;
  }
  printf(stdout, "Pouch: %s cgroup applied \n", container_name);
  return SUCCESS_CODE;

error:
  return -1;
}

pouch_status pouch_container_print_info(const char* container_name) {
  container_config conf;
  pouch_status status = SUCCESS_CODE;

  char container_name_to_use[CNTNAMESIZE];

  if (container_name == NULL) {
    if ((status = pouch_container_get_connected_name(container_name_to_use)) !=
        SUCCESS_CODE) {
      printf(stdout, "Pouch: no connected container\n");
      goto done;
    }
  } else {
    strcpy(container_name_to_use, container_name);
  }

  if ((status = pouch_cconf_read(container_name, &conf)) != SUCCESS_CODE) {
    printf(stdout, "Pouch: %s container not found\n", container_name);
    goto done;
  }

  char buf[256];
  char cgmax[MAX_PATH_LENGTH];
  char cgstat[MAX_PATH_LENGTH];

  strcpy(cgmax, POUCH_CGROUPS_DIR);
  strcat(cgmax, "/");
  strcat(cgmax, conf.container_name);
  strcat(cgmax, "/cpu.max");

  strcpy(cgstat, POUCH_CGROUPS_DIR);
  strcat(cgstat, "/");
  strcat(cgstat, conf.container_name);
  strcat(cgstat, "/cpu.stat");

  int cpu_max_fd = open(cgmax, O_RDWR);
  int cpu_stat_fd = open(cgstat, O_RDWR);

  if (pouch_container_is_attached()) {
    printf(stdout, "     Pouch container- %s: connected\n",
           conf.container_name);
  } else {
    printf(stdout, "     Pouch container- %s: started\n", conf.container_name);
  }

  printf(stdout, "tty - %d\n", conf.tty_num);
  printf(stdout, "pid - %d\n", conf.pid);
  printf(stdout, "image - %s\n", conf.image_name);
  printf(stdout, "     cgroups:\n");

  if (cpu_max_fd < 0 || cpu_stat_fd < 0) {
    printf(stdout, "None.\n");
    status = SUCCESS_CODE;
    goto done;
  }

  memset(buf, 0, sizeof(buf));
  if (read(cpu_max_fd, buf, sizeof(buf)) < 0) {
    status = ERROR_CODE;
    goto skip;
  }
  printf(stdout, "cpu.max:     \n%s\n", buf);
  memset(buf, 0, sizeof(buf));
  if (read(cpu_stat_fd, buf, sizeof(buf)) < 0) {
    status = ERROR_CODE;
    goto skip;
  }
  printf(stdout, "cpu.stat:     \n%s\n", buf);

skip:
  if (close(cpu_max_fd) < 0) {
    status = ERROR_CODE;
  }
  if (close(cpu_stat_fd) < 0) {
    status = ERROR_CODE;
  }
  if (status != SUCCESS_CODE) {
    printf(stdout, "Pouch: failed to read cgroup info\n");
    goto done;
  }

done:
  return status;
}

pouch_status pouch_containers_print_all() {
  int i;
  int tty_fd = 0;
  char buf[CNTNAMESIZE] = {0};
  int is_empty_flag = 0;
  int id = 1;
  printf(stderr, "     Pouch containers:\n");
  pouch_status status = SUCCESS_CODE;

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    if ((tty_fd = pouch_open_tty(i, O_RDWR)) < 0) {
      printf(stderr, "cannot open tty%d\n", i);
      status = TTY_OPEN_ERROR_CODE;
      goto error;
    }
    if ((status = pouch_pconf_get_ttyname(i, buf)) != SUCCESS_CODE) {
      printf(stderr, "failed to read pconf for tty%d\n", i);
      goto error;
    }
    if (!buf[0]) {
      continue;
    }
    is_empty_flag = 1;
    printf(stderr, "%d. %s : ", id++, buf);
    memset(buf, 0, sizeof(buf));

    if (is_connected_tty(tty_fd)) {
      printf(stderr, "connected \n");
    } else {
      printf(stderr, "started \n");
    }
    close(tty_fd);
    tty_fd = 0;
  }
  if (!is_empty_flag) {
    printf(stderr, "None.\n");
  }

error:
  if (tty_fd > 0) {
    if (close(tty_fd) < 0 && status == SUCCESS_CODE) {
      printf(stderr, "cannot close tty%d fd %d\n", i, tty_fd);
      status = TTY_CLOSE_ERROR_CODE;
    }
  }
  return status;
}

pouch_status pouch_container_get_connected_name(char* const cname) {
  int i;
  int tty_fd;
  char attached_container_name[CNTNAMESIZE] = {0};

  // Not including the console tty
  for (i = 0; i < MAX_TTY - 1; i++) {
    if ((tty_fd = pouch_open_tty(i, O_RDWR)) < 0) {
      printf(stderr, "cannot open tty%d fd\n", i);
      return TTY_OPEN_ERROR_CODE;
    }
    bool is_curr_tty_connected = is_connected_tty(tty_fd);
    if (close(tty_fd) < 0) {
      printf(stderr, "cannot close tty%d fd\n", i);
      return TTY_CLOSE_ERROR_CODE;
    }
    if (!is_curr_tty_connected) continue;

    if (pouch_pconf_get_ttyname(i, attached_container_name) != SUCCESS_CODE) {
      printf(stderr, "failed to read pconf or connected tty from tty%d\n", i);
      return ERROR_CODE;
    }
    break;
  }

  if (!attached_container_name[0]) {
    return CONTAINER_NOT_FOUND_CODE;
  }

  strcpy(cname, attached_container_name);
  return SUCCESS_CODE;
}

/*
 *   Prepare image mount path:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: container_name
 *   @output: image_mount_point
 */
static pouch_status prepare_image_mount_path(const char* const container_name,
                                             char* const image_mount_point) {
  if (mkdir_if_not_exist(IMAGE_MOUNT_DIR) != SUCCESS_CODE) {
    perror("Cannot create image mount dir");
    return ERROR_IMAGE_INVALID_CODE;
  }
  if (strlen(container_name) + strlen(IMAGE_MOUNT_DIR) > MAX_PATH_LENGTH) {
    perror("Container name is too long");
    return CONTAINER_NAME_TOO_LONG_CODE;
  }
  strcpy(image_mount_point, IMAGE_MOUNT_DIR);
  strcat(image_mount_point, container_name);
  return SUCCESS_CODE;
}

/*
 *   Prepare old root mount path: used when calling pivot_root to prepare a dir
 * for put_old argument
 *   @input: container_name
 *   @output: image_mount_point
 */
static pouch_status prepare_old_root_mount_path(const char* const base_dir,
                                                char* const old_root_path) {
  struct stat mnt_base_path;
  if (stat(base_dir, &mnt_base_path) < 0) {
    perror("Old root base dir doesn't exists");
    return FAILED_TO_CREATE_OLD_ROOT_DIR;
  }
  if (strlen(base_dir) + strlen(old_root_relative_path) > MAX_PATH_LENGTH) {
    perror("Old root path is tol long");
    return FAILED_TO_CREATE_OLD_ROOT_DIR;
  }
  strcpy(old_root_path, base_dir);
  strcat(old_root_path, old_root_relative_path);
  return SUCCESS_CODE;
}

/*
 *   Finding a tty:
 *   - Finds a free tty to be attached
 *   @input: none
 *   @output: tty_name
 */
static pouch_status find_free_tty(int* const tty_found) {
  int i;
  int tty_fd;
  *tty_found = -1;
  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    if ((tty_fd = pouch_open_tty(i, O_RDWR)) < 0) {
      printf(stderr, "cannot open tty%d\n", i);
      return TTY_OPEN_ERROR_CODE;
    }

    if (!is_attached_tty(tty_fd)) {
      *tty_found = i;
      close(tty_fd);
      return SUCCESS_CODE;
    }
    close(tty_fd);
  }

  return NO_AVAILABLE_TTY_ERROR_CODE;
}

/**
 * This function mkdirs and mounts all the mounts that are defined in the mounts
 * list of container above. It mounts them relatively to the mount_path, and the
 * image_path is the path to the image root fs. It does it in the order of the
 * mounts, so root fs should be placed first. It's opposit function,
 * pouch_container_umount, should be called to unmount the mounts.
 */
static pouch_status pouch_container_mount(
    const struct container_start_config* config) {
  int num_success = 0;
  pouch_status ret = SUCCESS_CODE;
  const struct container_mounts_def* cmount = config->mounts;

  char image_path[MAX_PATH_LENGTH];
  if (pouch_image_get_path(config->image_name, image_path) != SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to get image path\n");
    ret = ERROR_CODE;
    goto error;
  }

  for (; cmount->type != LIST_END; cmount++, num_success++) {
    char dest[MAX_PATH_LENGTH];
    strcpy(dest, config->image_mount_point);
    if (*cmount->dest != '/') {
      strcat(dest, "/");
    }
    strcat(dest, cmount->dest);
    if (cmount->type == IMAGE_ROOT_FS) {
      if (mkdir(dest) < 0) {
        printf(stderr, "Pouch: failed to create image root fs dir %s\n", dest);
        ret = MOUNT_IMAGE_ROOT_FS_FAILED_ERROR_CODE;
        goto error;
      }
      if (mount(image_path, dest, 0)) {
        unlink(dest);
        printf(stderr, "Pouch: failed to mount image root fs\n");
        ret = MOUNT_IMAGE_ROOT_FS_FAILED_ERROR_CODE;
        goto error;
      }
    } else if (cmount->type == BIND_MOUNT) {
      if (mkdir_if_not_exist(dest) != SUCCESS_CODE) {
        printf(stderr, "Pouch: failed to create bind mount dir %s\n", dest);
        ret = MOUNT_BIND_FAILED_ERROR_CODE;
        goto error;
      }
      POUCH_LOG_DEBUG("Pouch: mounting %s to %s\n", cmount->src, dest);
      if (mount(cmount->src, dest, "bind") < 0) {
        unlink(dest);
        printf(stderr, "Pouch: failed to bind mount %s\n", cmount->src);
        ret = MOUNT_BIND_FAILED_ERROR_CODE;
        goto error;
      }
    } else {
      printf(stderr, "Pouch: unknown mount type\n");
      ret = ERROR_CODE;
      goto error;
    }
  }
  return ret;

error:
  // Handle errors: iterate backwards, umount and unlink.
  for (; num_success > 0; num_success--, cmount--) {
    char dest[MAX_PATH_LENGTH];
    strcpy(dest, config->image_mount_point);
    strcat(dest, cmount->dest);
    if (umount(dest) < 0) {
      printf(stderr, "Pouch: failed to unmount %s\n", dest);
    }
    if (cmount->type == IMAGE_ROOT_FS) {
      if (unlink(dest) < 0) {
        printf(stderr, "Pouch: failed to unlink rootfs %s\n", dest);
      }
    }
  }

  return ret;
}

/**
 * This function umounts all the mounts that were mounted in the container,
 * relatively to the mount_path. It does it in reverse order of the mounts.
 * It also unlnks the mount_path, assuming a pivot_root was done to it,
 * and it is no longer in use (other mounts are under it, and cannot be
 * unlinked!).
 */
static pouch_status pouch_container_umount(
    const struct container_start_config* config) {
  // In reverse order. Find end:
  const struct container_mounts_def* cmount = config->mounts;
  int i = 0;
  while (cmount->type != LIST_END) {
    cmount++;
    i++;
  }
  bool failed = false;
  for (cmount--; i > 0; cmount--, i--) {
    char dest[MAX_PATH_LENGTH];
    strcpy(dest, config->image_mount_point);
    strcat(dest, cmount->dest);
    if (umount(dest) < 0) {
      printf(stderr, "Pouch: failed to unmount %s\n", dest);
      failed = true;
    }
    if (cmount->type == IMAGE_ROOT_FS) {
      if (unlink(dest) < 0) {
        printf(stderr, "Pouch: failed to unlink rootfs %s\n", dest);
        failed = true;
      }
    }
  }
  return failed ? ERROR_CODE : SUCCESS_CODE;
}

static pouch_status pouch_container_start_child(
    const struct container_start_config* config,
    mutex_t* const start_running_child_mutex,
    mutex_t* const allow_mount_cleanup_mutex,
    mutex_t* const container_started_mutex, int tty_fd) {
  pouch_status child_status = SUCCESS_CODE;
  int tty_attached = -1;
  char old_root_mount_point[MAX_PATH_LENGTH] = {0};
  // wait till the parent process finishes and releases the lock
  if (mutex_wait(start_running_child_mutex) != MUTEX_SUCCESS) {
    printf(stdout, "Pouch: failed to wait on parent mutex\n");
    child_status = POUCH_MUTEX_ERROR_CODE;
    goto child_error;
  }

  // Check if config file exists -- if not, parent init failed and we should
  // exit.
  container_config to_remove = {0};
  if (pouch_cconf_read(config->container_name, &to_remove) != SUCCESS_CODE) {
    printf(stderr, "Container %s initialization failed, child exiting\n",
           config->container_name);
    child_status = ERROR_CODE;
    goto child_error;
  }

  // Child process - setting up namespaces for the container
  // Set up mount namespace.
  if (unshare(MOUNT_NS) < 0) {
    printf(stdout, "Cannot create mount namespace\n");
    child_status = UNSAHRE_MNT_NS_FAILED_ERROR_CODE;
    goto child_error;
  }

  if (prepare_old_root_mount_path(config->image_mount_point,
                                  old_root_mount_point) < 0) {
    printf(stderr, "Pouch: failed to prepare old root mount dir at %s!\n",
           config->image_mount_point);
    child_status = FAILED_TO_CREATE_OLD_ROOT_DIR;
    goto child_error;
  }

  if (mkdir(old_root_mount_point) < 0) {
    printf(stderr, "Pouch: failed to create  old root mount dir at %s!\n",
           old_root_mount_point);
    child_status = FAILED_TO_CREATE_OLD_ROOT_DIR;
    goto child_error;
  }

  if (pivot_root(config->image_mount_point, old_root_mount_point) < 0) {
    printf(stderr, "Pouch: failed to pivot root to %s!\n",
           config->image_mount_point);
    child_status = FAILED_TO_PIVOT_ROOT;
    goto child_error;
  }

  if (chdir("/") < 0) {
    perror("Pouch: failed to change directory to / after pivot!");
    child_status = IMAGE_MOUNT_FAILED_ERROR_CODE;
    goto child_error;
  }

  // now, parent can clean up mounts outside the container.
  if (mutex_unlock(allow_mount_cleanup_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock child mutex");
    child_status = POUCH_MUTEX_ERROR_CODE;
    goto child_error;
  }

  // Unmount the old root mount point -- we're already chdir'd to the new
  // root!
  if (umount(old_root_relative_path) < 0) {
    printf(stderr, "Pouch: failed to umount old root mount dir at %s!\n",
           old_root_relative_path);
    child_status = FAILED_TO_CREATE_OLD_ROOT_DIR;
    goto child_error;
  }
  if (unlink(old_root_relative_path) < 0) {
    printf(stderr, "Pouch: failed to unlink old root mount dir at %s!\n",
           old_root_mount_point);
    child_status = FAILED_TO_CREATE_OLD_ROOT_DIR;
    goto child_error;
  }

  if (tty_fd >= 0) {
    ASSERT(config->daemonize);
    // attach stderr stdin stdout
    if ((child_status = attach_tty(tty_fd)) != SUCCESS_CODE) {
      printf(stderr, "attach failed - error in connecting to tty: %d\n",
             tty_fd);
      goto child_error;
    }
    tty_attached = tty_fd;
  } else {
    ASSERT(!config->daemonize);
  }

  POUCH_LOG_DEBUG("Entering container\n");
  if (mutex_unlock(container_started_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock container started mutex");
    child_status = POUCH_MUTEX_ERROR_CODE;
    goto child_error;
  }

  exit(config->child_func(config));

// unlock anyway in case of failure.
child_error:
  if (tty_attached >= 0) {
    detach_tty(tty_attached);
  }
  if (mutex_unlock(container_started_mutex) != MUTEX_SUCCESS)
    perror("Failed to unlock parent mutex");
  if (mutex_unlock(allow_mount_cleanup_mutex) != MUTEX_SUCCESS)
    perror("Failed to unlock parent mutex");
  if (tty_fd) {
    close(tty_fd);
  }
  if (*old_root_mount_point) {
    unlink(old_root_mount_point);
  }
  // if failed after the config was read, remove the config.
  if (*to_remove.container_name) {
    if (pouch_cconf_unlink(&to_remove) != SUCCESS_CODE) {
      perror("Failed to remove container config");
    }
  } else {
    perror("Failed to remove container config");
  }
  exit(child_status);
}

static int pouch_container_start_parent(
    const struct container_start_config* info, const int container_process_pid,
    const int tty_to_use, mutex_t* const start_running_child_mutex,
    mutex_t* const allow_mount_cleanup_mutex, const char* const cg_cname,
    container_config* const conf_out) {
  bool child_allowed_to_start = false, has_config_written = false;
  pouch_status parent_status = SUCCESS_CODE;
  int child_exit_status = 0;
  char cg_procs_path[MAX_PATH_LENGTH] = {0};

  // move container process to cgroup
  strcpy(cg_procs_path, cg_cname);
  strcat(cg_procs_path, "/cgroup.procs");
  int cgroup_procs_fd = open(cg_procs_path, O_RDWR);
  char cur_pid_buf[10];
  itoa(cur_pid_buf, container_process_pid);
  if (write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0) {
    close(cgroup_procs_fd);
    goto parent_end;
  }
  if (close(cgroup_procs_fd) < 0) {
    printf(stderr, "Failed to close cgroup_procs_fd\n");
    goto parent_end;
  }

  // Setup mounts
  if (pouch_container_mount(info) != SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to execute mounts for %s!\n",
           info->container_name);
    goto parent_end;
  }

  // Setup configuration for the new container
  conf_out->tty_num = tty_to_use;
  strcpy(conf_out->container_name, info->container_name);
  strcpy(conf_out->image_name, info->image_name);
  conf_out->pid = container_process_pid;
  if ((parent_status = pouch_cconf_write(conf_out)) != SUCCESS_CODE) {
    perror("Failed to write to cconf_out");
    goto parent_end;
  }
  has_config_written = true;

  // let the child process run
  if (mutex_unlock(start_running_child_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock parent mutex");
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto parent_end;
  }
  child_allowed_to_start = true;

  // and wait until the child process allows mount cleanup
  if (mutex_wait(allow_mount_cleanup_mutex) != MUTEX_SUCCESS) {
    perror("Failed to lock child mutex");
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto parent_end;
  }

  // cleanup mounts outside the container is now allowed.
  if (pouch_container_umount(info) != SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to execute umounts for %s!\n",
           info->container_name);
    parent_status = MOUNT_CLEANUP_FAILED_ERROR_CODE;
    goto parent_end;
  }

  // Wait for child to finish -- container exit.
  while (wait(&child_exit_status) != container_process_pid) {
    // wait for the child to finish
  }
  parent_status = WEXITSTATUS(child_exit_status);
  goto do_exit;

parent_end:
  // needs to fold config?
  if (has_config_written) {
    if (pouch_cconf_unlink(conf_out) != SUCCESS_CODE) {
      perror("Failed to remove container config");
    }
  }
  if (!child_allowed_to_start) {
    mutex_unlock(start_running_child_mutex);
  }

do_exit:
  if (info->daemonize) {
    exit(parent_status);
  } else {
    return parent_status;
  }
}

pouch_status _pouch_container_start(struct container_start_config* const info) {
  int tty_fd = -1;
  int container_process_pid = -1;
  int tty_to_use = -1;
  char cg_cname[MAX_PATH_LENGTH] = {0};
  char image_path[MAX_PATH_LENGTH] = {0};
  mutex_t start_running_child_mutex = {0}, global_pouch_mutex = {0},
          allow_mount_cleanup_mutex = {0}, container_started_mutex = {0};
  pouch_status parent_status = SUCCESS_CODE;
  container_config conf_out = {0};  // valid only when daemonize=0!

  // initialize the mutex for the parent (it will release the lock only when its
  // done).
  if (MUTEX_SUCCESS != mutex_init(&start_running_child_mutex) ||
      MUTEX_SUCCESS != mutex_init(&allow_mount_cleanup_mutex) ||
      MUTEX_SUCCESS != mutex_init(&container_started_mutex)) {
    printf(stdout,
           "Pouch: failed to create synchronization for container/global\n");
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto parent_error_no_unlock;
  }
  if (init_and_lock_pouch_global_mutex(&global_pouch_mutex) != SUCCESS_CODE) {
    perror("Failed to init pouch global mutex");
    return ERROR_CODE;
  }

  if (info->daemonize) {
    // Find tty name
    if ((parent_status = find_free_tty(&tty_to_use)) != SUCCESS_CODE) {
      printf(stdout, "Pouch: cannot create more containers\n");
      goto prep_error_unlock_global;
    }
  }

  // make sure image exists:
  if ((parent_status = pouch_image_exists(info->image_name)) != SUCCESS_CODE) {
    printf(stderr, "Pouch: image %s does not exist\n", info->image_name);
    goto prep_error_unlock_global;
  }

  if (pouch_cconf_read(info->container_name, NULL) == SUCCESS_CODE) {
    printf(stderr, "Pouch: %s container already exists!\n",
           info->container_name);
    goto prep_error_unlock_global;
  }

  printf(stdout, "Pouch: %s starting\n", info->container_name);

  // Move the current process to "/cgroup/<cname>" cgroup.
  // Prepare cgroup name for container
  if ((parent_status = prepare_cgroup_cname(info->container_name, cg_cname)) !=
      SUCCESS_CODE) {
    printf(stdout, "Pouch: failed to prepare cgroup name %s for %s\n", cg_cname,
           info->container_name);
    goto prep_error_unlock_global;
  }
  if ((parent_status = create_pouch_cgroup(cg_cname, info->container_name)) !=
      SUCCESS_CODE) {
    printf(stdout, "Pouch: failed to create cgroup %s for %s\n", cg_cname,
           info->container_name);
    goto prep_error_unlock_global;
  }

  if (tty_to_use >= 0) {
    // update cname in pouch conf
    if ((parent_status = pouch_pconf_write(tty_to_use, info->container_name)) <
        0) {
      printf(stdout, "Pouch: failed to write to pconf for %s\n",
             info->container_name);
      goto prep_error_unlock_global;
    }
    if ((tty_fd = pouch_open_tty(tty_to_use, O_RDWR)) < 0) {
      printf(stderr, "cannot open tty%d config\n", tty_to_use);
      parent_status = TTY_OPEN_ERROR_CODE;
      goto prep_error_unlock_global;
    }
  }

  // Prepare image mount point for container
  if ((parent_status = prepare_image_mount_path(
           info->container_name, info->image_mount_point)) != SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to prepare image mount point\n");
    goto prep_error_unlock_global;
  }

  if ((parent_status = pouch_image_get_path(info->image_name, image_path)) !=
      SUCCESS_CODE) {
    printf(stderr, "Pouch: failed to get image path for %s!\n",
           info->image_name);
    goto prep_error_unlock_global;
  }

  // initialize event mutexes.
  if (mutex_lock(&start_running_child_mutex) != MUTEX_SUCCESS ||
      mutex_lock(&allow_mount_cleanup_mutex) != MUTEX_SUCCESS ||
      mutex_lock(&container_started_mutex) != MUTEX_SUCCESS) {
    printf(stdout, "Pouch: failed to lock parent mutex\n");
    mutex_unlock(&global_pouch_mutex);
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto prep_error_locked;
  }

  // if daemonize=1, daemonize the container process (so it won't be realted to
  // the sh process)
  if (!info->daemonize || (info->daemonize && (fork() == 0))) {
    // Set up pid namespace before fork
    if (unshare(PID_NS) != 0) {
      printf(stderr, "Cannot create pid namespace\n");
      parent_status = UNSAHRE_PID_NS_FAILED_ERROR_CODE;
      goto prep_error_locked;
    }

    // create the container with new PID namespace
    if ((container_process_pid = fork()) == -1) {
      parent_status = POUCH_FORK_FAILED_ERROR_CODE;
      goto prep_error_locked;
    }

    if (container_process_pid == 0) {  // child
      pouch_container_start_child(info, &start_running_child_mutex,
                                  &allow_mount_cleanup_mutex,
                                  &container_started_mutex, tty_fd);

    } else {  // parent
      parent_status = pouch_container_start_parent(
          info, container_process_pid, tty_to_use, &start_running_child_mutex,
          &allow_mount_cleanup_mutex, cg_cname, &conf_out);
    }
  }

  // wait for container to be started.
  if (mutex_wait(&container_started_mutex) != MUTEX_SUCCESS) {
    perror("Failed to wait on container started mutex");
  }
  if (mutex_unlock(&global_pouch_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock global mutex");
  }

  // exit after parent executed.
  if (tty_fd >= 0) {
    if (close(tty_fd) < 0) return ERROR_CODE;
  }
  // If we have a config, it means we are not daemonizing the container.
  if (conf_out.container_name[0]) {
    ASSERT(!info->daemonize);  // just to be sure
    // remove all config files and clean up.
    if (_pouch_container_stop(&conf_out) != SUCCESS_CODE) {
      printf(stderr, "Pouch: failed to stop container %s\n",
             info->container_name);
    }
  }
  // if daemonize=1, it's just success by default. otherwise, return the parent
  // status.
  return parent_status;

prep_error_locked:
  if (mutex_unlock(&start_running_child_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock parent mutex");
  }
  if (mutex_unlock(&allow_mount_cleanup_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock parent mutex");
  }
  if (mutex_unlock(&container_started_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock parent mutex");
  }
prep_error_unlock_global:
  if (mutex_unlock(&global_pouch_mutex) != MUTEX_SUCCESS) {
    perror("Failed to unlock global mutex");
  }
parent_error_no_unlock:
  return parent_status;
}

pouch_status _pouch_container_stop(const container_config* const conf) {
  pouch_status ret = SUCCESS_CODE;
  int tty_fd = -1;
  char cur_pid_buf[10] = {0};
  bool is_daemon = conf->tty_num >= 0;

  // If it's a daemon, we need to stop the container process.
  if (is_daemon) {
    int cgroup_procs_fd = open("/cgroup/cgroup.procs", O_RDWR);
    itoa(cur_pid_buf, conf->pid);
    if (write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0) {
      close(cgroup_procs_fd);
      printf(stderr, "Failed to write to cgroup_procs_fd %s\n", cur_pid_buf);
      ret = FAILED_TO_WRITE_TO_CGROUP_PROCS_FD;
    }
    if (close(cgroup_procs_fd) < 0) {
      perror("Failed to close cgroup_procs_fd");
      ret = FAILED_TO_CLOSE_CGROUP_PROCS_FD;
    }
    if (kill(conf->pid) < 0) {
      printf(stderr, "Failed to kill container process with pid %d for %s\n",
             conf->pid, conf->container_name);
      ret = KILL_CONTAINER_ERROR_CODE;
      goto end;
    }
  }

  if ((ret = pouch_cconf_unlink(conf)) != SUCCESS_CODE) {
    printf(stderr, "Failed to unlink cconf %s\n", conf->container_name);
    goto end;
  }

  char cg_cname[MAX_PATH_LENGTH];

  if ((ret = prepare_cgroup_cname(conf->container_name, cg_cname)) !=
      SUCCESS_CODE) {
    perror("Failed to prepare cgroup name");
    goto end;
  }

  if (unlink(cg_cname) < 0) {
    perror("Failed to unlink cgroup");
    ret = UNLINK_CGROUP_ERROR_CODE;
  }
  if (is_daemon) {
    if ((ret = pouch_pconf_remove(conf->tty_num)) != SUCCESS_CODE) {
      perror("Failed to remove pconf");
      goto end;
    }
  }

  char mnt_point[MAX_PATH_LENGTH] = {0};
  if ((ret = prepare_image_mount_path(conf->container_name, mnt_point)) !=
      SUCCESS_CODE) {
    perror("Failed to prepare image mount path");
    goto end;
  }

  if (is_daemon) {
    if ((tty_fd = pouch_open_tty(conf->tty_num, O_RDWR)) < 0) {
      printf(stderr, "cannot open tty: tty%d\n", conf->tty_num);
      ret = TTY_OPEN_ERROR_CODE;
      goto end;
    }

    if (is_connected_tty(tty_fd)) {
      if (disconnect_tty(tty_fd) < 0) {
        ret = TTY_DISCONNECT_ERROR_CODE;
        goto end;
      }
    }
    if (detach_tty(tty_fd) < 0) {
      ret = TTY_DETACH_ERROR_CODE;
      goto end;
    }
  }

  if (is_daemon) {
    printf(stdout, "Pouch: %s destroyed\n", conf->container_name);
  } else {
    POUCH_LOG_DEBUG("Pouch: %s stopped\n", conf->container_name);
  }
  ret = SUCCESS_CODE;

end:
  if (tty_fd >= 0)
    if (close(tty_fd) < 0) return TTY_CLOSE_ERROR_CODE;
  return ret;
}

pouch_status pouch_container_connect(const char* const container_name) {
  pouch_status ret = SUCCESS_CODE;
  container_config conf;
  int tty_fd = -1;
  if ((ret = pouch_cconf_read(container_name, &conf)) != SUCCESS_CODE) {
    printf(stdout, "Pouch: %s container not found\n", container_name);
    ret = CONTAINER_NOT_FOUND_CODE;
    goto end;
  }

  if ((tty_fd = pouch_open_tty(conf.tty_num, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty: %d\n", conf.tty_num);
    ret = TTY_OPEN_ERROR_CODE;
    goto end;
  }

  if (!is_connected_tty(tty_fd)) {
    if (connect_tty(tty_fd) < 0) {
      close(tty_fd);
      printf(stderr, "cannot connect to the tty\n");
      ret = TTY_CONNECT_ERROR_CODE;
      goto end;
    }
  } else {
    printf(stdout, "Pouch: %s is already connected\n", container_name);
  }
  ret = SUCCESS_CODE;

end:
  if (tty_fd >= 0) {
    if (close(tty_fd) < 0) return TTY_CLOSE_ERROR_CODE;
  }
  return ret;
}

pouch_status pouch_container_disconnect() {
  container_config conf;
  char connected_cname[CNTNAMESIZE];
  int tty_fd = -1;

  pouch_status status = SUCCESS_CODE;

  if ((status = pouch_container_get_connected_name(connected_cname)) !=
      SUCCESS_CODE) {
    printf(stderr, "Pouch: no connected container\n");
    goto end;
  }

  if ((status = pouch_cconf_read(connected_cname, &conf)) != SUCCESS_CODE) {
    printf(stderr, "Pouch: cannot read from cconf for connected %s\n",
           connected_cname);
    goto end;
  }

  if ((tty_fd = pouch_open_tty(conf.tty_num, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty: %d\n", conf.tty_num);
    status = TTY_OPEN_ERROR_CODE;
    goto end;
  }

  if (disconnect_tty(tty_fd) < 0) {
    printf(stderr, "cannot disconnect from tty\n");
    status = TTY_DISCONNECT_ERROR_CODE;
    goto end;
  }

end:
  close(tty_fd);
  return status;
}
