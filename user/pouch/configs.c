#include "configs.h"

#include "container.h"
#include "fcntl.h"
#include "lib/user.h"
#include "param.h"
#include "util.h"

/*
 * Get tty device name by tty number (index).
 * For example, tty_num = 0 will return an absolute path to tty0.
 */
static void pouch_get_tty_name(const int tty_num, char dest[TTYNAMESIZE]) {
  strcpy(dest, DEV_DIR "ttyX");
  // replace X with char representation of tty_num
  dest[TTYNAMESIZE - 2] = tty_num + '0';
}

int pouch_open_tty(const int tty_num, const int mode) {
  char tty[TTYNAMESIZE];
  pouch_get_tty_name(tty_num, tty);
  return open(tty, mode);
}

/*
 * Get tty configuration file name by tty number (index).
 * For example, tty_num = 0 will return an absolute path to tty.c0.
 */
static void pouch_get_tty_conf_name(const int tty_num,
                                    char dest[MAX_PATH_LENGTH]) {
  strcpy(dest, POUCH_CONFIGS_DIR "tty.cX");
  // replace X with char representation of tty_num
  dest[sizeof(POUCH_CONFIGS_DIR "tty.cX") - 2] = tty_num + '0';
}

/*
 * Open tty configuration file by tty number (index).
 * For example, tty_num = 0 will return an absolute path to tty.c0.
 */
static int pouch_open_ttyc(const int tty_num, const int mode) {
  char ttyc[MAX_PATH_LENGTH] = {0};
  pouch_get_tty_conf_name(tty_num, ttyc);
  return open(ttyc, mode);
}

static void pouch_get_container_conf_name(const char* const container_name,
                                          char dest[MAX_PATH_LENGTH]) {
  strcpy(dest, POUCH_CONFIGS_DIR);
  strcat(dest, container_name);
}

/*
 * Open container configuration file by container name.
 */
static int pouch_open_container_file(const char* const container_name,
                                     const int mode) {
  char container_file[CNTNAMESIZE + sizeof(POUCH_CONFIGS_DIR)];
  pouch_get_container_conf_name(container_name, container_file);
  int fd = open(container_file, mode);
  if (fd < 0) {
    return -1;
  }

  return fd;
}

int pouch_cconf_unlink(const struct container_config* const conf) {
  char container_file[CNTNAMESIZE + sizeof(POUCH_CONFIGS_DIR)];
  pouch_get_container_conf_name(conf->container_name, container_file);
  return unlink(container_file);
}

pouch_status pouch_pconf_init() {
  // create a file for eacth tty that will hold cname inside currently connected
  // to it
  int i;
  int ttyc_fd;
  pouch_status status = SUCCESS_CODE;

  // Mkdir pouch common directory
  if (mkdir_if_not_exist(POUCH_CONFIGS_DIR) != SUCCESS_CODE) {
    perror("Invalid pouch configs directory.");
    status = ERROR_CODE;
    goto end;
  }

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    // check if cname ttys already created, if so, continue:
    if ((ttyc_fd = pouch_open_ttyc(i, O_RDWR)) >= 0) {
      goto close_and_continue;
    }

    if ((ttyc_fd = pouch_open_ttyc(i, O_CREATE | O_RDWR)) < 0) {
      printf(stderr, "cannot create tty conf %d\n", i);
      status = TTY_OPEN_ERROR_CODE;
      goto end;
    }

  close_and_continue:
    if (close(ttyc_fd) < 0) {
      printf(stderr, "cannot close tty %d fd (%d)\n", i, ttyc_fd);
      status = TTY_CLOSE_ERROR_CODE;
      goto end;
    }
  }
end:
  return status;
}

pouch_status pouch_pconf_write(const int tty_num, const char* const cname) {
  int ttyc_fd = -1;
  pouch_status status = SUCCESS_CODE;

  if ((ttyc_fd = pouch_open_ttyc(tty_num, O_CREATE | O_WRONLY)) < 0) {
    printf(stderr, "cannot open tty conf for tty%d\n", tty_num);
    status = TTY_OPEN_ERROR_CODE;
    goto end;
  }

  printf(ttyc_fd, "%s", cname);

end:
  if (ttyc_fd > 0) {
    if (close(ttyc_fd) < 0 && status == SUCCESS_CODE) {
      printf(stderr, "cannot close tty conf %d fd (%d)\n", tty_num, ttyc_fd);
      status = TTY_CLOSE_ERROR_CODE;
    }
  }
  return status;
}

pouch_status pouch_pconf_remove(const int tty_num) {
  int ttyc_fd;
  pouch_status status = SUCCESS_CODE;

  if ((ttyc_fd = pouch_open_ttyc(tty_num, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty conf for tty%d\n", tty_num);
    status = TTY_OPEN_ERROR_CODE;
    goto end;
  }
  if (close(ttyc_fd) < 0) {
    printf(stderr, "cannot close tty conf %d fd (%d)\n", tty_num, ttyc_fd);
    status = TTY_CLOSE_ERROR_CODE;
    goto end;
  }
  ttyc_fd = -1;

  char ttyc[MAX_PATH_LENGTH] = {0};
  pouch_get_tty_conf_name(tty_num, ttyc);
  if (unlink(ttyc) < 0) {
    printf(stderr, "cannot unlink %s conf for tty %d\n", ttyc, tty_num);
    status = ERROR_CODE;
    goto end;
  }

  if ((ttyc_fd = pouch_open_ttyc(tty_num, O_CREATE | O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    status = TTY_OPEN_ERROR_CODE;
  }

end:
  close(ttyc_fd);
  return status;
}

pouch_status pouch_pconf_get_ttyname(const int tty_num, char* const cname) {
  int ttyc_fd = -1;
  pouch_status status = SUCCESS_CODE;

  if ((ttyc_fd = pouch_open_ttyc(tty_num, O_RDWR)) < 0) {
    printf(stderr, "cannot open ttyc%s\n", tty_num);
    status = TTY_OPEN_ERROR_CODE;
    goto end;
  }

  if (read(ttyc_fd, cname, CNTNAMESIZE) < 0) {
    printf(stderr, "cannot read from ttyc%d fd %d\n", tty_num);
    status = ERROR_CODE;
    goto end;
  }

end:
  if (ttyc_fd > 0) {
    if (close(ttyc_fd) < 0 && status == SUCCESS_CODE) {
      printf(stderr, "cannot close ttyc%d fd %d\n", tty_num, ttyc_fd);
      status = TTY_CLOSE_ERROR_CODE;
    }
  }
  return SUCCESS_CODE;
}

/**
 * Reads a configuration file line, and parses it in the form of key: value.
 * key should include the colon.
 */
pouch_status read_key_str_line(const int fd, const char* const key,
                               char* const val, const int max_len) {
  char c;

  if (max_len <= 0) {
    return ERROR_CODE;
  }

  const char* keypos = key;
  // Match the key.
  for (; *keypos != '\0' && read(fd, &c, 1) == 1 && c == *keypos; ++keypos) {
  };
  if (*keypos != '\0') {
    return ERROR_CODE;
  }
  c = -1;
  // Skip whitespace, not newline
  while (read(fd, &c, 1) == 1 && isspace(c) && c != '\n') {
  };
  if (c == -1) {
    perror("read failed");
    return ERROR_CODE;
  }

  // Read the value itself until reaching a newline
  char* dest_pos = val;
  *(dest_pos++) = c;  // first char is from previous loop.
  for (; read(fd, (dest_pos++), 1) == 1 && *(dest_pos - 1) != '\n';) {
    if (dest_pos - val >= max_len) {
      perror("value too long");
      return ERROR_CODE;
    }
  }

  // newline reached.
  *(dest_pos - 1) = '\0';

  return SUCCESS_CODE;
}

/**
 * Reads a configuration file line, and parses it in the form of key: value.
 * key should include the colon.
 */
pouch_status read_key_int_line(const int fd, const char* const key,
                               int* const val) {
  char buf[10];
  if (read_key_str_line(fd, key, buf, sizeof(buf)) != SUCCESS_CODE) {
    return ERROR_CODE;
  }
  *val = atoi(buf);
  return SUCCESS_CODE;
}

pouch_status pouch_cconf_read(const char* const container_name,
                              container_config* const conf) {
  int cont_fd = -1;
  pouch_status status = SUCCESS_CODE;

  if ((cont_fd = pouch_open_container_file(container_name, O_RDWR)) < 0) {
    status = FAILED_TO_OPEN_CCONF_ERROR_CODE;
    goto end;
  }

  // No need to fill the config
  if (conf == NULL) {
    status = SUCCESS_CODE;
    goto end;
  }

  // parse tty num and pid:
  if (read_key_int_line(cont_fd, CONFIG_KEY_TTYNUM, &conf->tty_num) !=
          SUCCESS_CODE ||
      read_key_int_line(cont_fd, CONFIG_KEY_PPID, &conf->pid) != SUCCESS_CODE ||
      read_key_str_line(cont_fd, CONFIG_KEY_NAME, conf->container_name,
                        sizeof(conf->container_name)) != SUCCESS_CODE ||
      read_key_str_line(cont_fd, CONFIG_KEY_IMAGE, conf->image_name,
                        sizeof(conf->image_name)) != SUCCESS_CODE) {
    status = ERROR_CODE;
    goto end;
  }

  status = SUCCESS_CODE;

end:
  if (cont_fd >= 0) {
    if (close(cont_fd) < 0 && status == SUCCESS_CODE) {
      printf(stderr, "failed to close %s fd %d\n", container_name, cont_fd);
      status = FAILED_TO_CLOSE_CCONF_ERROR_CODE;
    }
  }
  return status;
}

pouch_status pouch_cconf_write(const container_config* conf) {
  if (conf == NULL) {
    printf(stderr, "conf is NULL\n");
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }
  if (strlen(conf->container_name) == 0) {
    printf(stderr, "container_name is empty\n");
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }
  if (conf->pid <= 0) {
    printf(stderr, "pid is %d <= 0!\n", conf->pid);
    return INVALID_CCONF_TO_WRITE_ERROR_CODE;
  }

  int cont_fd =
      pouch_open_container_file(conf->container_name, O_CREATE | O_RDWR);
  if (cont_fd < 0) {
    printf(stderr, "cannot open %s\n", conf->container_name);
    return FAILED_TO_OPEN_CCONF_ERROR_CODE;
  }
  printf(cont_fd, "%s %d\n%s %d\n%s %s\n%s %s\n", CONFIG_KEY_TTYNUM,
         conf->tty_num, CONFIG_KEY_PPID, conf->pid, CONFIG_KEY_NAME,
         conf->container_name, CONFIG_KEY_IMAGE, conf->image_name);
  close(cont_fd);
  return SUCCESS_CODE;
}
