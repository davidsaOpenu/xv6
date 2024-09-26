#include "container.h"

#include "configs.h"
#include "fcntl.h"
#include "image.h"
#include "lib/mutex.h"
#include "lib/user.h"
#include "ns_types.h"
#include "param.h"
#include "stat.h"

#define CONTAINERS_GLOBAL_LOCK_NAME "cnts_gllk"
#define IMAGE_MOUNT_DIR "/mnt/"
static const char* container_exec_path = "../../sh";
static const char* start_container_argv[] = {"sh", 0};

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

  int cont_fd = open(container_name, 0);
  if (cont_fd < 0) {
    printf(stderr, "There is no container: %s in a started stage\n",
           container_name);
    goto error;
  }
  if (close(cont_fd) < 0) {
    printf(stderr, "Cannot close %s\n", container_name);
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
  strcat(cgmax, container_name);
  strcat(cgmax, "/cpu.max");

  strcpy(cgstat, POUCH_CGROUPS_DIR);
  strcat(cgstat, "/");
  strcat(cgstat, container_name);
  strcat(cgstat, "/cpu.stat");

  int cpu_max_fd = open(cgmax, O_RDWR);
  int cpu_stat_fd = open(cgstat, O_RDWR);

  if (pouch_container_is_attached()) {
    printf(stdout, "     Pouch container- %s: connected\n", container_name);
  } else {
    printf(stdout, "     Pouch container- %s: started\n", container_name);
  }

  printf(stdout, "tty - %s\n", conf.tty_name);
  printf(stdout, "pid - %d\n", conf.pid);
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
  char tty[] = "/ttyX";
  char buf[CNTNAMESIZE] = {0};
  int is_empty_flag = 0;
  int id = 1;
  printf(stderr, "     Pouch containers:\n");
  pouch_status status = SUCCESS_CODE;

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    tty[4] = '0' + i;
    if ((tty_fd = open(tty, O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", tty);
      status = TTY_OPEN_ERROR_CODE;
      goto error;
    }
    if ((status = pouch_pconf_get_ttyname(tty, buf)) != SUCCESS_CODE) {
      printf(stderr, "failed to read pconf from %s\n", tty);
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
      printf(stderr, "cannot close %s fd\n", tty);
      status = TTY_CLOSE_ERROR_CODE;
    }
  }
  return status;
}

pouch_status pouch_container_get_connected_name(char* const cname) {
  int i;
  int tty_fd;
  char tty[] = "/ttyX";
  char buf[CNTNAMESIZE] = {0};
  bool found = false;

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1) && !found; i++) {
    tty[4] = '0' + i;
    if ((tty_fd = open(tty, O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", tty);
      return TTY_OPEN_ERROR_CODE;
    }

    if (is_connected_tty(tty_fd)) {
      pouch_pconf_get_ttyname(tty, buf);
      strcpy(cname, buf);
      found = true;
    }

    close(tty_fd);
  }

  if (!buf[0]) {
    return CONTAINER_NOT_FOUND_CODE;
  }
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
  struct stat mnt_base_path;
  if (stat(IMAGE_MOUNT_DIR, &mnt_base_path) < 0) {
    if (mkdir(IMAGE_MOUNT_DIR) < 0) {
      perror("Cannot create image mount dir");
      return ERROR_IMAGE_INVALID_CODE;
    }
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
 *   Finding a tty:
 *   - Finds a free tty to be attached
 *   @input: none
 *   @output: tty_name
 */
static pouch_status find_tty(char* const tty_name) {
  int i;
  int tty_fd;
  char tty[] = "/ttyX";

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    tty[4] = '0' + i;
    if ((tty_fd = open(tty, O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", tty);
      return TTY_OPEN_ERROR_CODE;
    }

    if (!is_attached_tty(tty_fd)) {
      strcpy(tty_name, tty);
      close(tty_fd);
      return SUCCESS_CODE;
    }
    close(tty_fd);
  }

  return NO_AVAILABLE_TTY_ERROR_CODE;
}

pouch_status pouch_container_start(const char* container_name,
                                   const char* const image_name) {
  int tty_fd = -1;
  int pid = -1;
  int pid2 = -1;
  char tty_name[10];
  char cg_cname[MAX_PATH_LENGTH];
  char image_mount_point[MAX_PATH_LENGTH];
  char image_path[MAX_PATH_LENGTH];
  int daemonize = 1;
  mutex_t parent_mutex, global_mutex;
  pouch_status parent_status = SUCCESS_CODE;

  // initialize the mutex for the parent (it will release the lock only when its
  // done)
  if (MUTEX_SUCCESS != mutex_init(&parent_mutex) ||
      MUTEX_SUCCESS !=
          mutex_init_named(&global_mutex, CONTAINERS_GLOBAL_LOCK_NAME)) {
    printf(stdout,
           "Pouch: failed to create synchronization for container/global\n");
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto parent_error_no_unlock;
  }

  // global mutex is held throughout the entire fork process.
  if (mutex_lock(&global_mutex) != MUTEX_SUCCESS) {
    printf(stdout, "Pouch: failed to lock global mutex\n");
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto parent_error_no_unlock;
  }
  if (mutex_lock(&parent_mutex) != MUTEX_SUCCESS) {
    printf(stdout, "Pouch: failed to lock parent mutex\n");
    mutex_unlock(&global_mutex);
    parent_status = POUCH_MUTEX_ERROR_CODE;
    goto prep_error_unlock_global;
  }

  // Find tty name
  if ((parent_status = find_tty(tty_name)) != SUCCESS_CODE) {
    printf(stdout, "Pouch: cannot create more containers\n");
    goto prep_error_locked;
  }

  // make sure image exists:
  if ((parent_status = pouch_image_exists(image_name)) != SUCCESS_CODE) {
    printf(stderr, "Pouch: image %s does not exist\n", image_name);
    goto prep_error_locked;
  }

  int cont_fd = open(container_name, 0);
  if (cont_fd < 0) {
    printf(stdout, "Pouch: %s starting\n", container_name);
    close(cont_fd);
  } else {
    printf(stderr, "Pouch: %s container is already started\n", container_name);
    parent_status = CONTAINER_ALREADY_STARTED_CODE;
    goto prep_error_locked;
  }

  // Prepare cgroup name for container
  if ((parent_status = prepare_cgroup_cname(container_name, cg_cname)) !=
      SUCCESS_CODE) {
    printf(stdout, "Pouch: failed to prepare cgroup name %s for %s\n", cg_cname,
           container_name);
    goto prep_error_locked;
  }
  if ((parent_status = create_pouch_cgroup(cg_cname, container_name)) !=
      SUCCESS_CODE) {
    printf(stdout, "Pouch: failed to create cgroup %s for %s\n", cg_cname,
           container_name);
    goto prep_error_locked;
  }

  // update cname in pouch conf
  if ((parent_status = pouch_pconf_write(tty_name, container_name)) < 0) {
    printf(stdout, "Pouch: failed to write to pconf for %s\n", container_name);
    goto prep_error_locked;
  }

  if ((tty_fd = open(tty_name, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty %s %d\n", tty_name, tty_fd);
    parent_status = TTY_OPEN_ERROR_CODE;
    goto prep_error_locked;
  }

  // if daemonize=1, daemonize the container process (so it won't be realted to
  // the sh process)
  if (!daemonize || (daemonize && (pid2 = fork()) == 0)) {
    // Set up pid namespace before fork
    if (unshare(PID_NS) != 0) {
      printf(stderr, "Cannot create pid namespace\n");
      parent_status = UNSAHRE_PID_NS_FAILED_ERROR_CODE;
      goto prep_error_locked;
    }

    // create the container with new PID namespace
    if ((pid = fork()) == -1) {
      parent_status = POUCH_FORK_FAILED_ERROR_CODE;
      goto prep_error_locked;
    }

    if (pid == 0) {  // child
      pouch_status child_status;
      // wait till the parent process finishes and releases the lock
      if (mutex_lock(&parent_mutex) != MUTEX_SUCCESS) {
        printf(stdout, "Pouch: failed to lock parent mutex\n");
        child_status = POUCH_MUTEX_ERROR_CODE;
        goto child_error;
      }
      // attach stderr stdin stdout
      if ((child_status = attach_tty(tty_fd)) != SUCCESS_CODE) {
        printf(stderr, "attach failed - error in connecting to tty: %d\n",
               tty_fd);
        goto child_error;
      }
      // "Child process - setting up namespaces for the container
      // Set up mount namespace.
      if (unshare(MOUNT_NS) < 0) {
        printf(stdout, "Cannot create mount namespace\n");
        child_status = UNSAHRE_MNT_NS_FAILED_ERROR_CODE;
        goto child_error;
      }

      // Prepare image mount point for container
      if ((child_status = prepare_image_mount_path(
               container_name, image_mount_point)) != SUCCESS_CODE) {
        printf(stderr, "Pouch: failed to prepare image mount point\n");
        goto child_error;
      }

      if (mkdir(image_mount_point) < 0) {
        printf(stderr, "Pouch: failed to create image mount point at %s!\n",
               image_mount_point);
        child_status = IMAGE_MOUNT_FAILED_ERROR_CODE;
        goto child_error;
      }

      if ((child_status = pouch_image_get_path(image_name, image_path)) !=
          SUCCESS_CODE) {
        printf(stderr, "Pouch: failed to get image path for %s!\n", image_name);
        goto child_error;
      }
      if (mount(image_path, image_mount_point, 0) < 0) {
        printf(stderr, "Pouch: failed to mount image %s at %s!\n", image_name,
               image_mount_point);
        child_status = IMAGE_MOUNT_FAILED_ERROR_CODE;
        goto child_error;
      }

      // TODO(Future): implement a pivot_root syscall and call it to the image
      // directory, so / filesystem is actually our image inside the started
      // container.
      if (chdir(image_mount_point) < 0) {
        printf(stderr, "Pouch: failed to change directory to %s!\n",
               image_mount_point);
        child_status = IMAGE_MOUNT_FAILED_ERROR_CODE;
        goto child_error;
      }

      // Child was created, now we can release the global lock!
      mutex_unlock(&global_mutex);
      printf(stderr, "Entering container\n");
      exec(container_exec_path, start_container_argv);
      // child image is replaced with sh, so this line should never be reached.

    // unlock anyway in case of failure.
    child_error:
      mutex_unlock(&global_mutex);
      exit(child_status);
    } else {  // parent
      // Move the current process to "/cgroup/<cname>" cgroup.
      strcat(cg_cname, "/cgroup.procs");
      int cgroup_procs_fd = open(cg_cname, O_RDWR);
      char cur_pid_buf[10];
      itoa(cur_pid_buf, pid);
      if (write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0) {
        close(cgroup_procs_fd);
        goto parent_error;
      }
      if (close(cgroup_procs_fd) < 0) goto parent_error;
      container_config conf;
      strcpy(conf.tty_name, tty_name);
      strcpy(conf.container_name, container_name);
      strcpy(conf.image_name, image_name);
      conf.pid = pid;
      if ((parent_status = pouch_cconf_write(&conf)) != SUCCESS_CODE) {
        perror("Failed to write to cconf");
        goto parent_error;
      }
      // let the child process run
      mutex_unlock(&parent_mutex);
      wait(0);
      exit(0);

    parent_error:
      mutex_unlock(&parent_mutex);
      return parent_status;
    }
  }

  // exit ok
  if (close(tty_fd) < 0) return -1;
  return SUCCESS_CODE;

prep_error_locked:
  mutex_unlock(&parent_mutex);
prep_error_unlock_global:
  mutex_unlock(&global_mutex);
parent_error_no_unlock:
  return parent_status;
}

pouch_status pouch_container_stop(const char* const container_name) {
  pouch_status ret = SUCCESS_CODE;
  container_config conf = {0};
  int tty_fd = -1;
  if ((ret = pouch_cconf_read(container_name, &conf)) != SUCCESS_CODE) {
    printf(stdout, "Pouch: %s container not found\n", container_name);
    return ret;
  }
  // Return the process to root cgroup.
  char cur_pid_buf[10];
  int cgroup_procs_fd = open("/cgroup/cgroup.procs", O_RDWR);
  itoa(cur_pid_buf, conf.pid);
  if (write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0) {
    close(cgroup_procs_fd);
    return -1;
  }
  if (close(cgroup_procs_fd) < 0) return -1;

  if (kill(conf.pid) < 0) {
    return KILL_CONTAINER_PROC_ERROR_CODE;
  }
  if (unlink(container_name) < 0) return -1;

  char cg_cname[MAX_PATH_LENGTH];

  if ((ret = prepare_cgroup_cname(container_name, cg_cname)) != SUCCESS_CODE) {
    return ret;
  }

  if (unlink(cg_cname) < 0) {
    return -1;
  }
  if ((ret = pouch_pconf_remove(conf.tty_name)) != SUCCESS_CODE) goto error;

  char mnt_point[MAX_PATH_LENGTH];
  if ((ret = prepare_image_mount_path(container_name, mnt_point)) !=
      SUCCESS_CODE) {
    return ret;
  }

  prepare_image_mount_path(container_name, mnt_point);
  // umount not needed as the container is already destroyed,
  // and mount lives only inside the container.
  if (unlink(mnt_point) < 0) {
    return -1;
  }

  tty_fd = open(conf.tty_name, O_RDWR);
  if (tty_fd < 0) {
    printf(stderr, "cannot open tty: %s\n", conf.tty_name);
    return TTY_OPEN_ERROR_CODE;
  }

  if (is_connected_tty(tty_fd)) {
    if (disconnect_tty(tty_fd) < 0) return TTY_DISCONNECT_ERROR_CODE;
  }
  if (detach_tty(tty_fd) < 0) return TTY_DETACH_ERROR_CODE;
  if (close(tty_fd) < 0) return TTY_CLOSE_ERROR_CODE;
  tty_fd = -1;

  printf(stdout, "Pouch: %s destroyed\n", container_name);
  return SUCCESS_CODE;

error:
  if (tty_fd >= 0) close(tty_fd);
  return ret;
}

pouch_status pouch_container_connect(const char* const container_name) {
  pouch_status ret = SUCCESS_CODE;
  container_config conf;
  if ((ret = pouch_cconf_read(container_name, &conf)) != SUCCESS_CODE) {
    printf(stdout, "Pouch: %s container not found\n", container_name);
    return ret;
  }

  int tty_fd = open(conf.tty_name, O_RDWR);
  if (tty_fd < 0) {
    printf(stderr, "cannot open tty: %s\n", conf.tty_name);
    return TTY_OPEN_ERROR_CODE;
  }

  if (!is_connected_tty(tty_fd)) {
    if (connect_tty(tty_fd) < 0) {
      close(tty_fd);
      printf(stderr, "cannot connect to the tty\n");
      return TTY_CONNECT_ERROR_CODE;
    }
  } else {
    printf(stdout, "Pouch: %s is already connected\n", container_name);
  }
  close(tty_fd);
  return SUCCESS_CODE;
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
    printf(stderr, "Pouch: cannot read from pconf\n");
    goto end;
  }

  if ((tty_fd = open(conf.tty_name, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty: %s\n", conf.tty_name);
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
