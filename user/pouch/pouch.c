#include "pouch.h"

#include "build.h"
#include "configs.h"
#include "container.h"
#include "fcntl.h"
#include "fs.h"
#include "image.h"
#include "lib/mutex.h"
#include "lib/user.h"
#include "ns_types.h"
#include "param.h"
#include "stat.h"

/*
 * Command line options
 */
#define POUCH_CMD_ARG_START "start"
#define POUCH_CMD_ARG_CONNECT "connect"
#define POUCH_CMD_ARG_DISCONNECT "disconnect"
#define POUCH_CMD_ARG_DESTROY "destroy"
#define POUCH_CMD_ARG_CGROUP "cgroup"
#define POUCH_CMD_ARG_INFO "info"
#define POUCH_CMD_ARG_LIST "list"
#define POUCH_CMD_ARG_IMAGES "images"
#define POUCH_CMD_ARG_BUILD "build"
#define POUCH_CMD_ARG_HELP "help"

#define BUILD_TAG_ARG_NAME "--tag"
#define BUILD_TAG_ARG_DEFAULT "default"
#define BUILD_FILE_ARG_NAME "--file"
#define BUILD_FILE_ARG_DEFAULT "Pouchfile"

/*
 *   Init pouch cgroup:
 *   - Creates root cgroup dir if not exists and mounts cgroups fs
 *   @input: none
 *   @output: none
 */
static pouch_status pouch_initialize_cgroup() {
  int cgroup_fd = -1;
  // check if cgoup filesystem already created
  if ((cgroup_fd = open(POUCH_CGROUPS_DIR, O_RDWR)) < 0) {
    if (mkdir(POUCH_CGROUPS_DIR) != 0) {
      printf(stdout, "Pouch: Failed to create root cgroup.\n");
      return MOUNT_CGROUP_FAILED_ERROR_CODE;
    }
    if (mount(0, POUCH_CGROUPS_DIR, "cgroup") != 0) {
      printf(stdout, "Pouch: Failed to mount cgroup fs.\n");
      return MOUNT_CGROUP_FAILED_ERROR_CODE;
    }
  } else {
    if (close(cgroup_fd) < 0) return ERROR_CODE;
  }

  return SUCCESS_CODE;
}

static void pouch_print_general_help() {
  perror("Pouch - container, image & cgroup manager for xv6\n");
  perror("General Pouch commands:\n");
  perror("       pouch " POUCH_CMD_ARG_HELP "\n");
  perror("          : displays this help message\n");
}

static void pouch_print_help_inside_container() {
  perror("\nPouch commands inside containers:\n");
  perror("       pouch " POUCH_CMD_ARG_DISCONNECT " \n");
  perror("          : disconnect a currently connected container\n");
  perror("       pouch " POUCH_CMD_ARG_INFO "\n");
  perror("          : query information about currently connected container\n");
}

void pouch_print_build_help() {
  perror("       pouch " POUCH_CMD_ARG_BUILD
         " [--file filename=Pouchfile] [--tag Tag=default]\n");
  perror(
      "          : build a new pouch image using the specified parameters\n");
  perror(
      "          - {--file} : The pouch file name to use for building the "
      "container.\n");
  perror("          - {--tag} : The tag to use for the output image\n");
}

void pouch_print_help_outside_cnt() {
  perror("\nPouch commands outside containers:\n");
  perror("       pouch " POUCH_CMD_ARG_START " {name} {image}\n");
  perror("          : starts a new container\n");
  perror("          - {name} : container name\n");
  perror("          - {image} : image name\n");
  perror("       pouch " POUCH_CMD_ARG_CONNECT " {name}\n");
  perror("          : connect already started container\n");
  perror("          - {name} : container name\n");
  perror("       pouch " POUCH_CMD_ARG_DESTROY " {name}\n");
  perror("          : destroy a container\n");
  perror("          - {name} : container name\n");
  perror("       pouch " POUCH_CMD_ARG_INFO " {name}\n");
  perror("          : query information about a container\n");
  perror("          - {name} : container name\n");
  perror("       pouch " POUCH_CMD_ARG_LIST "\n");
  perror("          : displays state of all created containers\n");
  perror("      \ncontainers cgroups:\n");
  perror("       pouch " POUCH_CMD_ARG_CGROUP
         " {cname} {state-object} [value]\n");
  perror("          : limit given cgroup state-object\n");
  perror("          - {name} : container name\n");
  perror("          - {state-object} : cgroups state-object. Refer spec.\n");
  perror(
      "          - [value] : argument for the state-object, multiple values "
      "delimited by ','\n");
  perror("      \npouch images:\n");
  perror("       pouch " POUCH_CMD_ARG_IMAGES "\n");
  perror("          : list pouch images in the system.\n");
  pouch_print_build_help();
}

static pouch_status pouch_cli_check_container_name(
    const char* const container_name) {
  if (container_name == NULL || strlen(container_name) == 0) {
    perror("Error: Container name must be specified.\n");
    return ERROR_CODE;
  }
  if (strlen(container_name) > CNTNAMESIZE) {
    printf(stderr,
           "Error: Container name invalid, must be 1-%d chars, got %d.\n",
           CNTNAMESIZE, strlen(container_name));
    return ERROR_CODE;
  }
  return SUCCESS_CODE;
}

static pouch_status pouch_cli_check_image_name(const char* const image_name) {
  if (image_name == NULL || strlen(image_name) == 0) {
    perror("Error: Image name must be specified.\n");
    return ERROR_CODE;
  }
  if (strlen(image_name) > IMG_NAME_SIZE) {
    printf(stderr, "Error: Image name invalid, must be 1-%d chars, got %d.\n",
           IMG_NAME_SIZE, strlen(image_name));
    return ERROR_CODE;
  }
  return SUCCESS_CODE;
}

static pouch_status pouch_cli_build_parse(const int argc,
                                          const char* const argv[],
                                          const char** const file_name,
                                          const char** const tag) {
  const char* const* options = &argv[0];
  /* Parse build options: --file, --tag */
  while (options < argv + argc) {
    if (strcmp(*options, BUILD_FILE_ARG_NAME) == 0) {
      if (options + 1 >= argv + argc) {
        printf(stderr,
               "Error: Expected file name after " BUILD_FILE_ARG_NAME "\n");
        goto error;
      }
      if (*file_name) {
        perror("Error: Specified more than one " BUILD_FILE_ARG_NAME
               " argument.\n");
        goto error;
      }
      *file_name = *(++options);
    } else if (strcmp(*options, BUILD_TAG_ARG_NAME) == 0) {
      if (options + 1 >= argv + argc) {
        printf(stderr,
               "Error: Expected tag name after " BUILD_TAG_ARG_NAME "\n");
        goto error;
      }
      if (*tag) {
        perror("Error: Specified more than one " BUILD_TAG_ARG_NAME
               " argument.\n");
        goto error;
      }
      *tag = *(++options);
    } else {
      printf(stderr, "Error: Unexpected argument %s!\n", *options);
      goto error;
    }
    ++options;
  }

  // Apply default arguments
  if (*tag == NULL) {
    *tag = BUILD_TAG_ARG_DEFAULT;
  }
  if (*file_name == NULL) {
    *file_name = BUILD_FILE_ARG_DEFAULT;
  }

  if (pouch_cli_check_image_name(*tag) != SUCCESS_CODE) {
    goto error;
  }
  return SUCCESS_CODE;

error:
  return ERROR_CODE;
}

static pouch_status pouch_cli_do_start(const p_cmd cmd, const int argc,
                                       const char* const argv[]) {
  pouch_status status = SUCCESS_CODE;
  if (cmd != P_CMD_START) {
    perror("Error: Invalid command callback (INTERNAL!).\n");
    return ERROR_CODE;
  }
  if (argc != 2) {
    printf(stderr, "Error: Invalid number of arguments %d.\n", argc);
    return ERROR_CODE;
  }
  if ((status = pouch_cli_check_container_name(argv[0])) != SUCCESS_CODE) {
    return status;
  }
  if ((status = pouch_cli_check_image_name(argv[1])) != SUCCESS_CODE) {
    return status;
  }
  return pouch_container_start(argv[0], argv[1]);
}

static pouch_status pouch_cli_do_container_operation(const p_cmd cmd,
                                                     const int argc,
                                                     const char* const argv[]) {
  pouch_status status = SUCCESS_CODE;
  if (argc != 1) {
    printf(stderr, "Error: Invalid number of arguments.\n", argc);
    return ERROR_CODE;
  }
  if ((status = pouch_cli_check_container_name(argv[0])) != SUCCESS_CODE) {
    return status;
  }
  switch (cmd) {
    case P_CMD_CONNECT:
      return pouch_container_connect(argv[0]);
    case P_CMD_DESTROY:
      return pouch_container_stop(argv[0]);
    case P_CMD_INFO_OUTSIDE:
      return pouch_container_print_info(argv[0]);
    default:
      perror("Error: Invalid command callback (INTERNAL!).\n");
      return ERROR_CODE;
  }
}

static pouch_status pouch_cli_do_global_op(const p_cmd cmd, const int argc,
                                           const char* const argv[]) {
  if (argc != 0) {
    printf(stderr,
           "Error: Invalid number of arguments (expected none, got %d).\n",
           argc);
    return ERROR_CODE;
  }

  switch (cmd) {
    case P_CMD_LIST:
      return pouch_containers_print_all();
    case P_CMD_IMAGES:
      return pouch_images_print();
    case P_CMD_DISCONNECT:
      return pouch_container_disconnect();
    case P_CMD_INFO_INSIDE:
      return pouch_container_print_info(NULL);
    default:
      perror("Error: Invalid command callback (INTERNAL!).\n");
      return ERROR_CODE;
  }
}

static pouch_status pouch_cli_do_limit(const p_cmd cmd, const int argc,
                                       const char* const argv[]) {
  pouch_status status = SUCCESS_CODE;
  if (cmd != P_CMD_LIMIT) {
    perror("Error: Invalid command callback (INTERNAL!).\n");
    return ERROR_CODE;
  }
  if (argc != 3) {
    perror("Error: Invalid number of arguments.\n");
    return ERROR_CODE;
  }
  if ((status = pouch_cli_check_container_name(argv[0])) != SUCCESS_CODE) {
    return status;
  }
  return pouch_container_limit(argv[0], argv[1], argv[2]);
}

static pouch_status pouch_cli_do_build(const p_cmd cmd, const int argc,
                                       const char* const argv[]) {
  const char* file_name = NULL;
  const char* tag = NULL;
  pouch_status status = SUCCESS_CODE;
  if (cmd != P_CMD_BUILD) {
    perror("Error: Invalid command callback (INTERNAL!).\n");
    return ERROR_CODE;
  }
  if ((status = pouch_cli_build_parse(argc, argv, &file_name, &tag)) !=
      SUCCESS_CODE) {
    return status;
  }
  return pouch_image_build(file_name, tag);
}

static pouch_status pouch_cli_do_help(const p_cmd cmd, const int argc,
                                      const char* const argv[]) {
  pouch_print_general_help();
  if (pouch_container_is_attached()) {
    pouch_print_help_inside_container();
  } else {
    pouch_print_help_outside_cnt();
  }
  return cmd == P_CMD_HELP ? SUCCESS_CODE : ERROR_CODE;
}

static const struct pouch_cli_command supported_pouch_commands[P_CMD_MAX] = {
    {.cmd = P_CMD_START,
     .callback = pouch_cli_do_start,
     .command_name = POUCH_CMD_ARG_START,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_CONNECT,
     .callback = pouch_cli_do_container_operation,
     .command_name = POUCH_CMD_ARG_CONNECT,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_DISCONNECT,
     .callback = pouch_cli_do_global_op,
     .command_name = POUCH_CMD_ARG_DISCONNECT,
     .inside_or_out = INSIDE_CONTAINER},
    {.cmd = P_CMD_DESTROY,
     .callback = pouch_cli_do_container_operation,
     .command_name = POUCH_CMD_ARG_DESTROY,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_LIMIT,
     .callback = pouch_cli_do_limit,
     .command_name = POUCH_CMD_ARG_CGROUP,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_INFO_OUTSIDE,
     .callback = pouch_cli_do_container_operation,
     .command_name = POUCH_CMD_ARG_INFO,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_INFO_INSIDE,
     .callback = pouch_cli_do_container_operation,
     .command_name = POUCH_CMD_ARG_INFO,
     .inside_or_out = INSIDE_CONTAINER},
    {.cmd = P_CMD_LIST,
     .callback = pouch_cli_do_global_op,
     .command_name = POUCH_CMD_ARG_LIST,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_IMAGES,
     .callback = pouch_cli_do_global_op,
     .command_name = POUCH_CMD_ARG_IMAGES,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_BUILD,
     .callback = pouch_cli_do_build,
     .command_name = POUCH_CMD_ARG_BUILD,
     .inside_or_out = OUTSIDE_CONTAINER},
    {.cmd = P_CMD_HELP,
     .callback = pouch_cli_do_help,
     .command_name = POUCH_CMD_ARG_HELP,
     .inside_or_out = INSIDE_AND_OUTSIDE_CONTAINER},
};

const struct pouch_cli_command* pouch_cli_get_command_from_args(
    const int argc, const char* const argv[]) {
  if (argc < 2) {
    printf(stderr, "Error: No command specified.\n");
    goto error;
  }

  bool inside_container = pouch_container_is_attached();
  bool has_name_match = false;
  for (int i = 0; i < P_CMD_MAX; i++) {
    if (strcmp(argv[1], supported_pouch_commands[i].command_name) == 0) {
      has_name_match = true;
      if (inside_container &&
          !(supported_pouch_commands[i].inside_or_out & INSIDE_CONTAINER)) {
        printf(stderr, "Error: Invalid command for inside container.\n");
        goto error;
      }
      if (!inside_container &&
          !(supported_pouch_commands[i].inside_or_out & OUTSIDE_CONTAINER)) {
        printf(stderr, "Error: Invalid command for outside container %d %d.\n",
               supported_pouch_commands[i].inside_or_out, INSIDE_CONTAINER);
        goto error;
      }
      return &supported_pouch_commands[i];
    }
  }

  if (!has_name_match) {
    printf(stderr, "Error: no command %s.\n", argv[1]);
  }

error:
  return NULL;
}

int main(int argc, const char* const argv[]) {
  enum pouch_exit_code exit_code = 0;
  pouch_status status = SUCCESS_CODE;

  const struct pouch_cli_command* spec =
      pouch_cli_get_command_from_args(argc, argv);
  if (spec == NULL) {
    exit_code = POUCH_EXIT_FAILURE;
    goto exit;
  }

  if ((status = pouch_initialize_cgroup()) != SUCCESS_CODE) {
    perror("Pouch: Failed to initialize cgroup.\n");
    exit_code = status;
    goto exit;
  }
  if ((status = pouch_pconf_init()) != SUCCESS_CODE) {
    perror("Pouch: Failed to initialize pconf.\n");
    exit_code = status;
    goto exit;
  }

  /* call the command - parse, execute, and return. Skip `binary command`
   * arguments. */
  argv += 2;
  argc -= 2;
  status = spec->callback(spec->cmd, argc, argv);

  exit_code = status == SUCCESS_CODE ? POUCH_EXIT_SUCCESS : POUCH_EXIT_FAILURE;

exit:
  exit(exit_code);
}
