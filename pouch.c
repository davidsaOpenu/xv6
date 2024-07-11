#include "pouch.h"

#include "fcntl.h"
#include "fs.h"
#include "mutex.h"
#include "ns_types.h"
#include "param.h"
#include "stat.h"
#include "user.h"

#define POUCH_GLOBAL_LOCK_NAME "pouch_glk"
#define IMAGE_MOUNT_DIR "/mnt/"

#define CONFIG_KEY_PPID "PPID:"
#define CONFIG_KEY_NAME "NAME:"
#define CONFIG_KEY_IMAGE "IMAGE:"

/*
 *   Helper consts
 */
enum POUCH_INTERNAL_STATUS_CODES {
  SUCCESS_CODE = 0,
  END_OF_FILE_CODE = 1,
  ERROR_CODE = -1
};
#define LINE_BUFFER_SIZE 1024
#define POUCHFILE_IMPORT_TOKEN "IMPORT"
#define POUCHFILE_RUN_TOKEN "RUN"

/*
 * Command line options
 */
#define POUCH_CMD_ARG_IMAGES "images"
#define POUCH_CMD_ARG_BUILD "build"

/*
 *   Helper types
 */
struct pouchfile_command {
  char* command;
  struct pouchfile_command* next;
};

struct pouchfile {
  char* image_name;
  struct pouchfile_command* commands_list_head;
};

/*
 *   Helper functions
 */
static void empty_string(char* s, int length) {
  for (int i = 0; i < length; i++) s[i] = 0;
}

void panic(char* s) {
  printf(stderr, "%s\n", s);
  exit(1);
}

static int is_whitespace(const char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/*
 *   Pouchfile functions
 */
static int next_line(const int pouchfile_fd, char** const line,
                     int* const count) {
  int exit_code = SUCCESS_CODE;
  *count = 0;
  *line = (char*)malloc(sizeof(char) * LINE_BUFFER_SIZE);
  if (*line == NULL) {
    return ERROR_CODE;
  }

  int allocated_line_length = LINE_BUFFER_SIZE;
  char current = 0;
  int index = 0;

  int read_ret = 0;
  bool started_read = false;
  while ((read_ret = read(pouchfile_fd, &current, 1)) > 0) {
    /* Skip initial whitespace */
    if (!started_read) {
      if (is_whitespace(current)) {
        continue;
      }
      started_read = true;
    }

    if (current == '\n' || current == '\0') {
      break;
    }

    (*line)[index] = current;
    ++index;

    if (index == allocated_line_length) {
      const int last_line_length = allocated_line_length;
      allocated_line_length += LINE_BUFFER_SIZE;
      char* new_line = (char*)malloc(sizeof(char) * allocated_line_length);
      if (new_line == NULL) {
        goto exit_error;
      }

      memmove(new_line, *line, last_line_length);
      free(*line);
      *line = new_line;
    }
  }

  if (read_ret == 0) {
    exit_code = END_OF_FILE_CODE;
  }
  (*line)[index] = '\0';
  *count = index;
  return exit_code;

exit_error:
  free(*line);
  *line = NULL;
  return ERROR_CODE;
}

static int pouchfile_init(struct pouchfile** const pouchfile,
                          const char* const image_name) {
  *pouchfile = NULL;

  if (image_name == NULL) {
    printf(stderr, "Empty image name!\n");
    goto error_image_name;
  }

  int image_name_length = strlen(image_name);
  if (image_name_length == 0) {
    printf(stderr, "Empty image name!\n");
    goto error_image_name;
  }

  *pouchfile = (struct pouchfile*)malloc(sizeof(struct pouchfile));
  if (*pouchfile == NULL) {
    printf(stderr, "Pouchfile struct alloc failed!\n");
    return ERROR_CODE;
  }

  (*pouchfile)->image_name =
      (char*)malloc(sizeof(char) * (image_name_length + 1));
  if ((*pouchfile)->image_name == NULL) {
    printf(stderr, "Image name alloc failed!\n");
    goto error_image_name;
  }

  strcpy((*pouchfile)->image_name, image_name);
  (*pouchfile)->commands_list_head = NULL;
  return SUCCESS_CODE;

error_image_name:
  free(pouchfile);
  return ERROR_CODE;
}

static void pouchfile_destroy(struct pouchfile** const pouchfile) {
  free((*pouchfile)->image_name);
  struct pouchfile_command* current_command = (*pouchfile)->commands_list_head;
  while (current_command != NULL) {
    struct pouchfile_command* const to_free = current_command;
    current_command = current_command->next;
    free(to_free->command);
    free(to_free);
  }

  *pouchfile = NULL;
}

static int pouchfile_add_command(struct pouchfile* const pouchfile,
                                 const char* const command_string) {
  const int new_command_length = strlen(command_string);
  if (new_command_length == 0) {
    return SUCCESS_CODE;
  }

  struct pouchfile_command* const new_command =
      (struct pouchfile_command*)malloc(sizeof(struct pouchfile_command));
  if (new_command == NULL) {
    return ERROR_CODE;
  }

  char* const new_command_string =
      (char*)malloc(sizeof(char) * (new_command_length + 1));
  if (new_command_string == NULL) {
    goto error_new_command;
  }

  strcpy(new_command_string, command_string);
  new_command->command = new_command_string;
  new_command->next = NULL;
  if (pouchfile->commands_list_head == NULL) {
    pouchfile->commands_list_head = new_command;
  } else {
    struct pouchfile_command* current = pouchfile->commands_list_head;
    while (current->next != NULL) {
      current = current->next;
    }

    current->next = new_command;
  }

  return SUCCESS_CODE;

error_new_command:
  free(new_command);
  return ERROR_CODE;
}

static char* pouchfile_skip_cmd(char* line, const char* pouchfile_token) {
  char* const end_of_original_line = line + strlen(line);

  char* next_token = strtok_r(line, " \t\n\r\f", NULL);
  if (next_token == NULL) {
    return NULL;
  }
  if (strcmp(next_token, pouchfile_token) == 0) {
    char* to_return = next_token + strlen(next_token);
    // Skip whitespace after token, if there are some.
    while (to_return < end_of_original_line && is_whitespace(*to_return)) {
      ++to_return;
    }
    if (to_return < end_of_original_line)
      return to_return + 1;
    else
      return to_return;
  }

  return NULL;
}

static int pouch_pouchfile_parse(const char* const pouchfile_path,
                                 struct pouchfile** const pouchfile) {
  int exit_code = SUCCESS_CODE;
  const int pouchfile_fd = open(pouchfile_path, O_RDONLY);
  if (pouchfile_fd < 0) {
    printf(stderr, "Failed to open pouchfile %s\n", pouchfile_path);
    return ERROR_CODE;
  }

  char* import_line = NULL;
  int import_line_length = 0;
  enum POUCH_INTERNAL_STATUS_CODES read_line_code = SUCCESS_CODE;

  /* Extract import, skip empty lines, break on end-of-file. */
  while (read_line_code == SUCCESS_CODE) {
    if ((read_line_code = next_line(pouchfile_fd, &import_line,
                                    &import_line_length)) == ERROR_CODE) {
      printf(stderr, "Failed to read import line from Pouchfile\n");
      goto image_line_error;
    }
    if (import_line_length > 0) {
      break;
    }
  }

  if (import_line_length == 0) {
    printf(stderr, "No import line found in Pouchfile\n");
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }

  char* const image_name_start =
      pouchfile_skip_cmd(import_line, POUCHFILE_IMPORT_TOKEN);
  if (image_name_start == NULL) {
    printf(stderr,
           "Failed to find import directive in first line of Pouchfile: %s\n",
           import_line);
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }
  if (*image_name_start == '\0' ||
      import_line + import_line_length < image_name_start) {
    printf(stderr,
           "Failed to find image name for import directive in first line of "
           "Pouchfile: %s\n",
           import_line);
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }

  if (pouchfile_init(pouchfile, image_name_start) == ERROR_CODE) {
    printf(stderr, "Failed to init pouchfile struct\n");
    exit_code = ERROR_CODE;
    goto pouchfile_creation_error;
  }

  free(import_line);
  import_line = NULL;

  char* run_command_line = NULL;
  int run_command_line_length = 0;
  int extract_line_status = SUCCESS_CODE;
  while (extract_line_status == SUCCESS_CODE) {
    if ((extract_line_status = next_line(pouchfile_fd, &run_command_line,
                                         &run_command_line_length)) ==
        ERROR_CODE) {
      printf(stderr, "Failed to extract run line from Pouchfile\n");
      exit_code = ERROR_CODE;
      goto pouch_commands_error;
    }

    if (run_command_line_length == 0) {
      goto skip_line;
    }

    char* const run_command_start =
        pouchfile_skip_cmd(run_command_line, POUCHFILE_RUN_TOKEN);
    if (run_command_start == NULL) {
      printf(stderr,
             "Failed to find run directive in first line of Pouchfile\n");
      exit_code = ERROR_CODE;
      goto pouch_commands_add_error;
    }
    if (*run_command_start == '\0') {
      printf(stderr,
             "Failed to find run argument in first line of Pouchfile\n");
      exit_code = ERROR_CODE;
      goto pouch_commands_error;
    }

    if (pouchfile_add_command(*pouchfile, run_command_start) == ERROR_CODE) {
      exit_code = ERROR_CODE;
      goto pouch_commands_error;
    }

  skip_line:
    free(run_command_line);
  }
  run_command_line = NULL;

pouch_commands_add_error:
  free(run_command_line);
pouch_commands_error:
  if (exit_code != SUCCESS_CODE) {
    pouchfile_destroy(pouchfile);
  }
pouchfile_creation_error:
  free(import_line);
image_line_error:
  close(pouchfile_fd);
  return exit_code;
}

/*
 *   Pouch functions
 */
static int pouch_limit_cgroup(char* container_name, char* cgroup_state_obj,
                              char* limitation) {
  char cg_limit_cname[256];

  prepare_cgroup_cname(container_name, cg_limit_cname);
  strcat(cg_limit_cname, "/");
  strcat(cg_limit_cname, cgroup_state_obj);

  int cont_fd = open(container_name, 0);
  if (cont_fd < 0) {
    printf(stderr, "There is no container: %s in a started stage\n",
           container_name);
    goto error;
  }
  int cname_fd = open(cg_limit_cname, O_RDWR);
  if (cname_fd < 0) {
    printf(stderr, "Incorrect cgroup object-state provided. Not applied.\n",
           container_name);
    goto error;
  }

  if (write(cname_fd, limitation, sizeof(limitation)) < 0) return -1;
  if (close(cname_fd) < 0) return -1;
  printf(1, "Pouch: %s cgroup applied \n", container_name);
  return 0;

error:
  return -1;
}

static void prepare_cgroup_cname(const char* container_name, char* cg_cname) {
  if (strlen(container_name) + strlen("/cgroup/") > MAX_PATH_LENGTH) {
    panic("Container name is too long");
  }
  strcpy(cg_cname, "/cgroup/");
  strcat(cg_cname, container_name);
}

static void prepare_image_mount_path(const char* container_name,
                                     char* image_mount_point) {
  struct stat mnt_base_path;
  if (stat(IMAGE_MOUNT_DIR, &mnt_base_path) < 0) {
    if (mkdir(IMAGE_MOUNT_DIR) < 0) {
      panic("Cannot create image mount dir");
    }
  }
  if (strlen(container_name) + strlen(IMAGE_MOUNT_DIR) > MAX_PATH_LENGTH) {
    panic("Container name is too long");
  }
  strcpy(image_mount_point, IMAGE_MOUNT_DIR);
  strcat(image_mount_point, container_name);
}

static void image_name_to_path(const char* image_name, char* image_path) {
  if (strlen(image_name) + strlen(IMAGE_DIR) > MAX_PATH_LENGTH) {
    panic("Image name is too long");
  }
  strcpy(image_path, IMAGE_DIR);
  strcat(image_path, image_name);
}

static int image_exists(char* image_name) {
  char buf[MAX_PATH_LENGTH];
  struct stat st;

  image_name_to_path(image_name, buf);
  if (stat(buf, &st) < 0) {
    return -1;
  }
  return 0;
}

static char* fmtname(char* path) {
  static char buf[DIRSIZ + 1];
  char* p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--) {
  }
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

static int pouch_print_images() {
  char buf[MAX_PATH_LENGTH], *p;
  char dir[MAX_PATH_LENGTH];
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(IMAGE_DIR, 0)) < 0) {
    printf(2, "Cannot access the images dir, make sure the path %s exists\n",
           IMAGE_DIR);
    return -1;
  }

  if (fstat(fd, &st) < 0) {
    printf(2, "cannot stat the images dir, make sure the path %s exists \n",
           IMAGE_DIR);
    close(fd);
    return -1;
  }

  bool printed_first = false;
  if (st.type != T_DIR) {
    printf(stderr, "%s should be a directory\n", IMAGE_DIR);
    return -1;
  }

  strcpy(buf, IMAGE_DIR);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (stat(buf, &st) < 0) {
      printf(1, "Cannot stat %s\n", buf);
      continue;
    }
    strcpy(dir, fmtname(buf));
    if (strncmp(dir, ".", 1) != 0) {
      if (!printed_first) {
        printf(1, "Pouch images available:\n");
        printed_first = true;
      }
      printf(1, "%s\n", dir);
    }
  }

  if (!printed_first) {
    printf(1, "No images available\n");
  }

  close(fd);
  return 0;
}

static int pouch_cmd(char* container_name, char* image_name, enum p_cmd cmd) {
  int tty_fd;
  int pid;
  char tty_name[10];
  char cg_cname[256];
  char mnt_point[256];

  if (cmd == START) {
    return pouch_fork(container_name, image_name);
  }

  if (cmd == LIST) {
    if (print_clist() < 0) {
      return -1;
    }
    return 0;
  }

  if (cmd == IMAGES) {
    if (pouch_print_images() < 0) {
      return -1;
    }
    return 0;
  }

  if (read_from_cconf(container_name, tty_name, &pid, image_name) < 0) {
    return -1;
  }

  if (cmd == INFO) {
    if (print_cinfo(container_name, tty_name, pid) < 0) {
      return -1;
    }
    return 0;
  }

  if ((tty_fd = open(tty_name, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty: %s\n", tty_name);
    return -1;
  }

  if (cmd == DESTROY && pid != 0) {
    // Return the process to root cgroup.
    char cur_pid_buf[10];
    int cgroup_procs_fd = open("/cgroup/cgroup.procs", O_RDWR);
    itoa(cur_pid_buf, pid);
    if (write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0) return -1;
    if (close(cgroup_procs_fd) < 0) return -1;

    if (kill(pid) < 0) {
      return -1;
    }
    if (unlink(container_name) < 0) return -1;

    prepare_cgroup_cname(container_name, cg_cname);

    if (unlink(cg_cname) < 0) {
      return -1;
    }
    if (remove_from_pconf(tty_name) < 0) return -1;

    prepare_image_mount_path(container_name, mnt_point);
    // umount not needed as the container is already destroyed,
    // and mount lives only inside the container.
    if (unlink(mnt_point) < 0) {
      return -1;
    }

    if (is_connected_tty(tty_fd)) {
      if (disconnect_tty(tty_fd) < 0) return -1;
    }
    if (detach_tty(tty_fd) < 0) return -1;
    if (close(tty_fd) < 0) return -1;

    printf(1, "Pouch: %s destroyed\n", container_name);
    return 0;
  }

  if (cmd == CONNECT) {
    if (!is_connected_tty(tty_fd)) {
      if (connect_tty(tty_fd) < 0) {
        close(tty_fd);
        printf(stderr, "cannot connect to the tty\n");
        return -1;
      }
    } else {
      printf(1, "Pouch: %s is already connected\n", container_name);
    }

  } else if (cmd == DISCONNECT && disconnect_tty(tty_fd) < 0) {
    close(tty_fd);
    printf(stderr, "cannot disconnect from tty\n");
    return -1;
  }

  close(tty_fd);
  return 0;
}

static int find_tty(char* tty_name) {
  int i;
  int tty_fd;
  char tty[] = "/ttyX";

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    tty[4] = '0' + i;
    if ((tty_fd = open(tty, O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", tty);
      return -1;
    }

    if (!is_attached_tty(tty_fd)) {
      strcpy(tty_name, tty);
      close(tty_fd);
      return 0;
    }
    close(tty_fd);
  }

  return -1;
}

static int init_pouch_conf() {
  // create a file for eacth tty that will hold cname inside currently connected
  // to it
  int i;
  int ttyc_fd;
  char ttyc[] = "tty.cX";

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    ttyc[5] = '0' + i;
    // check if cname ttys already created
    if (open(ttyc, O_RDWR) > 0) continue;
    if ((ttyc_fd = open(ttyc, O_CREATE | O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", ttyc);
      return -1;
    }
    if (close(ttyc_fd) < 0) return -1;
  }
  return 0;
}

static int write_to_pconf(char* ttyname, char* cname) {
  char ttyc[] = "tty.cX";
  int ttyc_fd;
  ttyc[5] = ttyname[4];
  if ((ttyc_fd = open(ttyc, O_CREATE | O_WRONLY)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    return -1;
  }
  printf(ttyc_fd, "%s", cname);
  close(ttyc_fd);
  return 0;
}

static int remove_from_pconf(char* ttyname) {
  char ttyc[] = "tty.cX";
  int ttyc_fd;
  ttyc[5] = ttyname[4];
  if ((ttyc_fd = open(ttyc, O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    return -1;
  }
  if (unlink(ttyc) < 0) return -1;
  if ((ttyc_fd = open(ttyc, O_CREATE | O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    return -1;
  }
  close(ttyc_fd);
  return 0;
}

static int read_from_pconf(char* ttyname, char* cname) {
  char ttyc[] = "/tty.cX";
  int ttyc_fd;
  ttyc[6] = ttyname[4];
  if ((ttyc_fd = open(ttyc, O_RDWR)) < 0) {
    printf(stderr, "cannot open %s fd\n", ttyc);
    return -1;
  }
  read(ttyc_fd, cname, CNTNAMESIZE);
  close(ttyc_fd);
  return 0;
}

static int print_clist() {
  int i;
  int tty_fd;
  char tty[] = "/ttyX";
  char buf[CNTNAMESIZE] = {0};
  int is_empty_flag = 0;
  int id = 1;
  printf(stderr, "     Pouch containers:\n");

  // Not including the console tty
  for (i = 0; i < (MAX_TTY - 1); i++) {
    tty[4] = '0' + i;
    if ((tty_fd = open(tty, O_RDWR)) < 0) {
      printf(stderr, "cannot open %s fd\n", tty);
      return -1;
    }
    if (read_from_pconf(tty, buf) < 0) {
      printf(stderr, "failed to read pconf from %s\n", tty);
      return -1;
    }
    if (!buf[0]) {
      continue;
    }
    is_empty_flag = 1;
    printf(stderr, "%d. %s : ", id++, buf);
    empty_string(buf, sizeof(buf));

    if (is_connected_tty(tty_fd)) {
      printf(stderr, "connected \n");
    } else {
      printf(stderr, "started \n");
    }
    close(tty_fd);
  }
  if (!is_empty_flag) {
    printf(stderr, "None.\n");
  }

  return 0;
}

static int get_connected_cname(char* cname) {
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
      return -1;
    }

    if (is_connected_tty(tty_fd)) {
      read_from_pconf(tty, buf);
      strcpy(cname, buf);
      found = true;
    }

    close(tty_fd);
  }

  if (!buf[0]) {
    return -1;
  }
  return 0;
}

static int read_from_cconf(char* container_name, char* tty_name, int* pid,
                           char* image_name) {
  char pid_str[sizeof("PPID:") + 10];
  int cont_fd = -1;

  char container_file[CNTNAMESIZE + sizeof("/")];
  strcpy(container_file, "/");
  strcat(container_file, container_name);
  cont_fd = open(container_file, 0);
  if (cont_fd < 0) {
    printf(stderr, "There is no container: %s in a started stage\n",
           container_name);
    goto error;
  }

  if (read(cont_fd, tty_name, sizeof("/ttyX")) < sizeof("/ttyX")) {
    printf(stderr, "CONT TTY NOT FOUND\n");
    goto error;
  }

  tty_name[sizeof("/ttyX") - 1] = 0;

  int i = 0;
  char c;
  while (read(cont_fd, &c, 1) > 0) {
    if (c == '\n') {
      pid_str[i] = 0;
      break;
    }
    pid_str[i++] = c;
  }
  pid_str[i] = '\0';

  // make sure it begins with PPID:
  if (strncmp(pid_str, "PPID:", sizeof("PPID:") - 1) != 0) {
    printf(stderr, "CONT PID NOT FOUND\n");
    goto error;
  }

  // parse the pid
  *pid = atoi(pid_str + sizeof("PPID:"));

  // skip container name "NAME: container_name":
  i = 0;
  while (read(cont_fd, &c, 1) > 0) {
    if (c == '\n') {
      break;
    }
  }

  // read image name
  i = 0;
  while (read(cont_fd, &c, 1) > 0) {
    if (c == '\n') {
      image_name[i] = 0;
      break;
    }
    image_name[i++] = c;
  }
  image_name[i] = '\0';

  close(cont_fd);
  return 0;

error:
  if (cont_fd >= 0) close(cont_fd);
  return -1;
}

static int write_to_cconf(char* container_name, char* tty_name, int pid,
                          char* image_name) {
  int cont_fd = open(container_name, O_CREATE | O_RDWR);
  if (cont_fd < 0) {
    printf(stderr, "cannot open %s\n", container_name);
    return -1;
  }
  printf(cont_fd, "%s\n%s %d\n%s %s\n%s %s\n", tty_name, CONFIG_KEY_PPID, pid,
         CONFIG_KEY_NAME, container_name, CONFIG_KEY_IMAGE, image_name);
  close(cont_fd);
  return 0;
}

static int pouch_fork(char* container_name, char* image_name) {
  int tty_fd = -1;
  int pid = -1;
  int pid2 = -1;
  char tty_name[10];
  char cg_cname[256];
  char image_mount_point[256];
  char image_path[256];
  int daemonize = 1;
  mutex_t parent_mutex, global_mutex;

  // initialize the mutex for the parent (it will release the lock only when its
  // done)
  if (MUTEX_SUCCESS != mutex_init(&parent_mutex) ||
      MUTEX_SUCCESS !=
          mutex_init_named(&global_mutex, POUCH_GLOBAL_LOCK_NAME)) {
    printf(1, "Pouch: failed to create synchronization for container/global\n");
    exit(1);
  }

  // global mutex is held throughout the entire fork process.
  mutex_lock(&global_mutex);
  mutex_lock(&parent_mutex);

  // Find tty name
  if (find_tty(tty_name) < 0) {
    printf(1, "Pouch: cannot create more containers\n");
    mutex_unlock(&global_mutex);
    exit(1);
  }

  // make sure image exists:
  if (image_exists(image_name) < 0) {
    printf(stderr, "Pouch: image %s does not exist\n", image_name);
    mutex_unlock(&global_mutex);
    exit(1);
  }

  int cont_fd = open(container_name, 0);
  if (cont_fd < 0) {
    printf(1, "Pouch: %s starting\n", container_name);
    close(cont_fd);
  } else {
    mutex_unlock(&global_mutex);
    printf(stderr, "Pouch: %s container is already started\n", container_name);
    exit(1);
  }

  // Prepare cgroup name for container
  prepare_cgroup_cname(container_name, cg_cname);
  if (create_pouch_cgroup(cg_cname, container_name) < 0) {
    mutex_unlock(&global_mutex);
    exit(1);
  }

  // update cname in pouch conf
  write_to_pconf(tty_name, container_name);

  if ((tty_fd = open(tty_name, O_RDWR)) < 0) {
    printf(stderr, "cannot open tty %s %d\n", tty_name, tty_fd);
    mutex_unlock(&global_mutex);
    return -1;
  }

  // if daemonize=1, daemonize the container process (so it won't be realted to
  // the sh process)
  if (!daemonize || (daemonize && (pid2 = fork()) == 0)) {
    // Set up pid namespace before fork
    if (unshare(PID_NS) != 0) {
      printf(stderr, "Cannot create pid namespace\n");
      mutex_unlock(&global_mutex);
      return -1;
    }

    // create the container with new PID namespace
    pid = fork();
    if (pid == -1) {
      mutex_unlock(&global_mutex);
      panic("fork");
    }
    if (pid == 0) {
      // wait till the parent process finishes and releases the lock
      mutex_lock(&parent_mutex);
      // attach stderr stdin stdout
      if (attach_tty(tty_fd) < 0) {
        printf(stderr, "attach failed - error in connecting to tty: %d\n",
               tty_fd);
        goto child_error;
      }
      // "Child process - setting up namespaces for the container
      // Set up mount namespace.
      if (unshare(MOUNT_NS) < 0) {
        printf(1, "Cannot create mount namespace\n");
        goto child_error;
      }

      // Prepare image mount point for container
      prepare_image_mount_path(container_name, image_mount_point);
      if (mkdir(image_mount_point) < 0) {
        printf(stderr, "Pouch: failed to create image mount point at %s!\n",
               image_mount_point);
        mutex_unlock(&global_mutex);
        exit(1);
      }
      image_name_to_path(image_name, image_path);
      if (mount(image_path, image_mount_point, 0) < 0) {
        printf(stderr, "Pouch: failed to mount image %s at %s!\n", image_name,
               image_mount_point);
        mutex_unlock(&global_mutex);
        exit(1);
      }

      // TODO(Future): implement a pivot_root syscall and call it to the image
      // directory, so / filesystem is actually our image inside the started
      // container.
      if (chdir(image_mount_point) < 0) {
        printf(stderr, "Pouch: failed to change directory to %s!\n",
               image_mount_point);
        mutex_unlock(&global_mutex);
        exit(1);
      }

      // Child was created, now we can release the global lock!
      mutex_unlock(&global_mutex);
      printf(stderr, "Entering container\n");
      exec("../../sh", argv);

    // unlock anyway in case of failure.
    child_error:
      mutex_unlock(&global_mutex);
      exit(1);
    } else {
      // "Parent process - waiting for child

      // Move the current process to "/cgroup/<cname>" cgroup.
      strcat(cg_cname, "/cgroup.procs");
      int cgroup_procs_fd = open(cg_cname, O_RDWR);
      char cur_pid_buf[10];
      itoa(cur_pid_buf, pid);
      if (write(cgroup_procs_fd, cur_pid_buf, sizeof(cur_pid_buf)) < 0)
        goto parent_error;
      if (close(cgroup_procs_fd) < 0) goto parent_error;
      if (write_to_cconf(container_name, tty_name, pid, image_name) >= 0) {
        // let the child process run
        mutex_unlock(&parent_mutex);
        wait(0);
      }

      exit(0);

    parent_error:
      mutex_unlock(&global_mutex);
      return -1;
    }
  }

  if (close(tty_fd) < 0) return -1;
  return 0;
}

static int print_help_inside_cnt() {
  int retval = 0;
  retval = printf(stderr, "\nPouch commands inside containers:\n");
  retval |= printf(stderr, "       pouch disconnect \n");
  retval |= printf(stderr,
                   "          : disconnect a currently connected container\n");
  retval |= printf(stderr, "       pouch info\n");
  retval |= printf(
      stderr,
      "          : query information about currently connected container\n");
  return retval;
}

void print_pouch_build_help() {
  printf(
      stderr,
      "       pouch build [--file filename=Pouchfile] [--tag Tag=default]\n");
  printf(
      stderr,
      "          : build a new pouch image using the specified parameters\n");
  printf(stderr,
         "          - {--file} : The pouch file name to use for building the "
         "container.\n");
  printf(stderr, "          - {--tag} : The tag to use for the output image\n");
}

void print_help_outside_cnt() {
  printf(stderr, "\nPouch commands outside containers:\n");
  printf(stderr, "       pouch start {name} {image}\n");
  printf(stderr, "          : starts a new container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "          - {image} : image name\n");
  printf(stderr, "       pouch connect {name}\n");
  printf(stderr, "          : connect already started container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "       pouch destroy {name}\n");
  printf(stderr, "          : destroy a container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "       pouch info {name}\n");
  printf(stderr, "          : query information about a container\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr, "       pouch list all\n");
  printf(stderr, "          : displays state of all created containers\n");
  printf(stderr, "      \ncontainers cgroups:\n");
  printf(stderr, "       pouch cgroup {cname} {state-object} [value]\n");
  printf(stderr, "          : limit given cgroup state-object\n");
  printf(stderr, "          - {name} : container name\n");
  printf(stderr,
         "          - {state-object} : cgroups state-object. Refer spec.\n");
  printf(stderr,
         "          - [value] : argument for the state-object, multiple values "
         "delimited by ','\n");
  printf(stderr, "      \npouch images:\n");
  printf(stderr, "       pouch images\n");
  printf(stderr, "          : list pouch images in the system.\n");
  print_pouch_build_help();
}

static int create_pouch_cgroup(char* cg_cname, char* cname) {
  if (mkdir(cg_cname) != 0) {
    printf(stderr,
           "Pouch: Failed to create cgroup with the given name. Consider "
           "another container name: %s \n",
           cname);
    return -1;
  }
  char cgpath[256];
  memset(cgpath, '\0', 256);
  strcpy(cgpath, cg_cname);
  strcat(cgpath, "/cgroup.subtree_control");

  int cgroup_subtree_control_fd = open(cgpath, O_RDWR);

  if (cgroup_subtree_control_fd < 0) return -1;

  // Enable cpu controller
  char buf[256];
  memset(buf, '\0', 256);
  strcpy(buf, "+cpu");
  if (write(cgroup_subtree_control_fd, buf, sizeof(buf)) < 0) return -1;
  if (close(cgroup_subtree_control_fd) < 0) return -1;
  return 0;
}

static int init_pouch_cgroup() {
  int cgroup_fd = -1;
  // check if cgoup filesystem already created
  if ((cgroup_fd = open("/cgroup", O_RDWR)) < 0) {
    if (mkdir("/cgroup") != 0) {
      printf(1, "Pouch: Failed to create root cgroup.\n");
      return -1;
    }
    if (mount(0, "/cgroup", "cgroup") != 0) {
      printf(1, "Pouch: Failed to mount cgroup fs.\n");
      return -1;
    }
  } else {
    if (close(cgroup_fd) < 0) return -1;
  }

  return 0;
}

static int print_cinfo(char* container_name, char* tty_name, int pid) {
  char buf[256];
  char cgmax[256];
  char cgstat[256];

  strcpy(cgmax, "/cgroup/");
  strcat(cgmax, container_name);
  strcat(cgmax, "/cpu.max");

  strcpy(cgstat, "/cgroup/");
  strcat(cgstat, container_name);
  strcat(cgstat, "/cpu.stat");

  int cpu_max_fd = open(cgmax, O_RDWR);
  int cpu_stat_fd = open(cgstat, O_RDWR);
  int ppid = getppid();

  if (ppid == 1) {
    printf(1, "     Pouch container- %s: connected\n", container_name);
  } else {
    printf(1, "     Pouch container- %s: started\n", container_name);
  }

  printf(1, "tty - %s\n", tty_name);
  printf(1, "pid - %d\n", pid);
  printf(1, "     cgroups:\n");
  if (cpu_max_fd > 0 && cpu_stat_fd > 0) {
    empty_string(buf, sizeof(buf));
    if (read(cpu_max_fd, buf, sizeof(buf)) < 0) return -1;
    printf(1, "cpu.max:     \n%s\n", buf);
    empty_string(buf, sizeof(buf));
    if (read(cpu_stat_fd, buf, sizeof(buf)) < 0) return -1;
    printf(1, "cpu.stat:     \n%s\n", buf);

    if (close(cpu_max_fd) < 0) return -1;
    if (close(cpu_stat_fd) < 0) return -1;
  } else {
    printf(1, "None.\n");
  }

  return 0;
}

static int pouch_build_parse_args(const int argc, char* argv[],
                                  char** const file_name, char** const tag) {
  char** options = &argv[2];
  /* Parse build options: --file, --tag */
  while (options < argv + argc) {
    if (strcmp(*options, "--file") == 0) {
      if (options + 1 >= argv + argc) {
        printf(stderr, "Error: Expected file name after --file\n");
        goto error;
      }
      if (*file_name) {
        printf(stderr, "Error: Specified more than one --file argument.\n");
        goto error;
      }
      *file_name = *(++options);
    } else if (strcmp(*options, "--tag") == 0) {
      if (options + 1 >= argv + argc) {
        printf(stderr, "Error: Expected tag name after --tag\n");
        goto error;
      }
      if (*tag) {
        printf(stderr, "Error: Specified more than one --tag argument.\n");
        goto error;
      }
      *tag = *(++options);
    } else {
      printf(stderr, "Error: Unexpected argument %s!\n", *options);
      goto error;
    }
    ++options;
  }
  return SUCCESS_CODE;

error:
  return ERROR_CODE;
}

static int pouch_build(char* file_name, char* tag) {
  if (!tag) {
    tag = "default";  // todo: getcwd?
  }
  if (!file_name) {
    file_name = "Pouchfile";
  }
  printf(stderr, "Building pouch image from \"%s\" to tag \"%s\"...\n",
         file_name, tag);
  struct pouchfile* pouchfile = NULL;
  if (pouch_pouchfile_parse(file_name, &pouchfile) == ERROR_CODE) {
    printf(stderr, "Error parsing Pouchfile %s\n", file_name);
    return ERROR_CODE;
  }
  // Future: Implement image construction!
  (void)pouchfile;

  pouchfile_destroy(&pouchfile);
  printf(stderr, "Built image to tag \"%s\".\n", tag);
  return SUCCESS_CODE;
}

int main(int argc, char* argv[]) {
  enum p_cmd cmd = START;
  char container_name[CNTNAMESIZE];
  char image_name[CNTNAMESIZE];

  // get parent pid
  int ppid = getppid();

  // check if any argument is --help and print help message accordingly:
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      if (ppid == 1)
        print_help_inside_cnt();
      else
        print_help_outside_cnt();
      exit(0);
    }
  }

  if (argc >= 4) {
    int arg_len = strlen(argv[3]);
    if (arg_len > CNTNAMESIZE || arg_len == 0) {
      printf(stderr, "Error: Image name invalid, must be 1-%d chars, got %d.\n",
             CNTNAMESIZE, arg_len);
      exit(1);
    }
    strcpy(image_name, argv[3]);
  }
  if (argc >= 3) {
    int arg_len = strlen(argv[2]);
    if (arg_len > CNTNAMESIZE || arg_len == 0) {
      printf(stderr,
             "Error: Container name invalid, must be 1-%d chars, got %d.\n",
             CNTNAMESIZE, arg_len);
      exit(1);
    }
    strcpy(container_name, argv[2]);
  } else if (argc == 2) {
    if (strcmp(argv[1], POUCH_CMD_ARG_IMAGES) != 0 &&
        strcmp(argv[1], POUCH_CMD_ARG_BUILD) != 0) {
      if (ppid == 1 && get_connected_cname(container_name) < 0) {
        print_help_inside_cnt();
        exit(1);
      } else if (ppid != 1) {
        print_help_outside_cnt();
        exit(0);
      }
    }
  } else {
    if (ppid == 1)
      print_help_inside_cnt();
    else
      print_help_outside_cnt();
    exit(0);
  }
  // get command type
  if (argc >= 2) {
    if ((strcmp(argv[1], "start")) == 0 && argc == 4) {
      cmd = START;
    } else if ((strcmp(argv[1], "connect")) == 0) {
      cmd = CONNECT;
    } else if ((strcmp(argv[1], "disconnect")) == 0) {
      cmd = DISCONNECT;
      if (ppid != 1) {
        printf(1, "Pouch: no container is connected\n");
        exit(1);
      }
    } else if ((strcmp(argv[1], "destroy")) == 0) {
      cmd = DESTROY;
    } else if ((strcmp(argv[1], "cgroup")) == 0 && argc == 5) {
      cmd = LIMIT;
    } else if ((strcmp(argv[1], "info")) == 0) {
      cmd = INFO;
    } else if ((strcmp(argv[1], "list")) == 0 &&
               (strcmp(argv[2], "all")) == 0) {
      cmd = LIST;
    } else if ((strcmp(argv[1], POUCH_CMD_ARG_IMAGES)) == 0) {
      cmd = IMAGES;
    } else if ((strcmp(argv[1], POUCH_CMD_ARG_BUILD)) == 0) {
      cmd = BUILD;
    } else {
      if (ppid == 1)
        print_help_inside_cnt();
      else
        print_help_outside_cnt();
      exit(1);
    }

    if (init_pouch_cgroup() < 0) {
      printf(1, "Pouch: cgroup operation failed.\n");
      exit(1);
    }

    if (init_pouch_conf() < 0) {
      printf(1, "Pouch: operation failed.\n");
      exit(1);
    }

    // Inside the container the are only few commands permitted, disable others.
    if (ppid == 1 && cmd != LIMIT && cmd != DISCONNECT /* && cmd != LIST*/
        && cmd != INFO && cmd != IMAGES && cmd != BUILD) {
      if (cmd == START) {
        printf(1, "Nesting containers is not supported.\n");
        goto error_exit;
      } else if (cmd == CONNECT) {
        printf(1, "Nesting containers is not supported.\n");
        goto error_exit;
      } else if (cmd == DESTROY) {
        printf(1, "Container can't be destroyed while connected.\n");
        goto error_exit;
      } else if (cmd == LIST) {
        if (print_help_inside_cnt() < 0) {
          goto error_exit;
        }
      }
    } else {
      // command execution
      if (cmd == LIMIT && argc == 5) {
        if (pouch_limit_cgroup(container_name, argv[3], argv[4]) < 0) {
          goto error_exit;
        }
      } else if (cmd == BUILD) {
        char* pouch_file_name = NULL;
        char* image_tag = NULL;
        if (pouch_build_parse_args(argc, argv, &pouch_file_name, &image_tag) !=
            SUCCESS_CODE) {
          printf(stderr, "\n");
          print_pouch_build_help();
          goto error_exit;
        }
        if (pouch_build(pouch_file_name, image_tag) != SUCCESS_CODE) {
          goto error_exit;
        }
      } else if (pouch_cmd(container_name, image_name, cmd) < 0) {
        printf(1, "Pouch: operation failed.\n");
        goto error_exit;
      }
    }
  }
  exit(0);
error_exit:
  exit(1);
}
