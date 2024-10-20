// Pouch build command implementation.
// Includes pouchfile parsing and execution.

#include "build.h"

#include "container.h"
#include "fcntl.h"
#include "image.h"
#include "lib/mutex.h"
#include "pouch.h"
#include "util.h"

#define LINE_BUFFER_SIZE (1024)

#define BUILD_BIND_DIR "/.build/"
#define BUILD_CONTAINER_NAME_SUFFIX ".build"

const struct command_to_type command_to_type_map[] = {
    {POUCHFILE_IMPORT_TOKEN, POUCHFILE_IMPORT},
    {POUCHFILE_RUN_TOKEN, POUCHFILE_RUN},
    {POUCHFILE_COPY_TOKEN, POUCHFILE_COPY},
    {NULL, COMMAND_NONE},
};

static pouch_status next_line(const int pouchfile_fd, char** const line,
                              int* const count) {
  pouch_status exit_code = SUCCESS_CODE;
  *count = 0;
  *line = (char*)malloc(sizeof(char) * LINE_BUFFER_SIZE);
  if (*line == NULL) {
    return ERROR_OUT_OF_MEMORY_CODE;
  }

  int allocated_line_length = LINE_BUFFER_SIZE;
  char current = 0;
  int index = 0;

  int read_ret = 0;
  bool started_read = false;
  while ((read_ret = read(pouchfile_fd, &current, 1)) > 0) {
    /* Skip initial whitespace */
    if (!started_read) {
      if (isspace(current)) {
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
        exit_code = ERROR_OUT_OF_MEMORY_CODE;
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

/** Allocates a new pouchfile structure. */
static pouch_status pouchfile_alloc(struct pouchfile** const pouchfile) {
  *pouchfile = NULL;

  *pouchfile = (struct pouchfile*)malloc(sizeof(struct pouchfile));
  if (*pouchfile == NULL) {
    printf(stderr, "Pouchfile struct alloc failed!\n");
    return ERROR_OUT_OF_MEMORY_CODE;
  }
  (*pouchfile)->commands_list_head = NULL;
  (*pouchfile)->ncmds = 0;

  return SUCCESS_CODE;
}

/** Frees a pouchfile structure. */
static void pouch_build_pouchfile_destroy(struct pouchfile** const pouchfile) {
  if (*pouchfile == NULL) {
    return;
  }
  struct pouchfile_command* current_command = (*pouchfile)->commands_list_head;
  while (current_command != NULL) {
    struct pouchfile_command* const to_free = current_command;
    current_command = current_command->next;
    free(to_free->command);
    free(to_free);
  }

  *pouchfile = NULL;
}

// ==== Parser functions ====
static pouch_status parse_run_command(
    struct pouchfile_command* const new_command,
    const struct pouchfile* const pouchfile, const char* const argument) {
  new_command->command = strdup(argument);
  if (new_command->command == NULL) {
    return ERROR_OUT_OF_MEMORY_CODE;
  }
  return SUCCESS_CODE;
}

static pouch_status parse_copy_command(
    struct pouchfile_command* const new_command,
    const struct pouchfile* const pouchfile, const char* const argument) {
  new_command->files_to_copy = strdup(argument);
  if (new_command->files_to_copy == NULL) {
    return ERROR_OUT_OF_MEMORY_CODE;
  }
  return SUCCESS_CODE;
}

static pouch_status parse_import_command(
    struct pouchfile_command* const new_command,
    const struct pouchfile* const pouchfile, const char* const argument) {
  new_command->image_name = strdup(argument);
  if (new_command->image_name == NULL) {
    return ERROR_OUT_OF_MEMORY_CODE;
  }
  return SUCCESS_CODE;
}

struct command_type_to_handler {
  const enum pouchfile_command_type command_type;
  pouch_status (*handler)(struct pouchfile_command* const new_command,
                          const struct pouchfile* const pouchfile,
                          const char* const argument);
};

static const struct command_type_to_handler command_parsers[] = {
    {POUCHFILE_RUN, parse_run_command},
    {POUCHFILE_COPY, parse_copy_command},
    {POUCHFILE_IMPORT, parse_import_command},
    {COMMAND_NONE, NULL},
};

/**
 * Adds a new command to the pouchfile by it's handler and argument.
 * Calls the handler functions and inserts to the command list.
 */
static pouch_status pouchfile_add_command(
    struct pouchfile* const pouchfile,
    const struct command_type_to_handler* const handler,
    const char* const argument) {
  pouch_status exit_code = SUCCESS_CODE;

  // Currently, all commands require an argument.
  const int new_command_length = strlen(argument);
  if (new_command_length == 0) {
    return ERROR_CODE;
  }

  // Allocate command struct.
  struct pouchfile_command* new_command =
      (struct pouchfile_command*)malloc(sizeof(struct pouchfile_command));
  if (new_command == NULL) {
    exit_code = ERROR_CODE;
    goto end;
  }
  new_command->type = handler->command_type;
  // Find handler for command, and call it.
  if ((exit_code = handler->handler(new_command, pouchfile, argument)) !=
      SUCCESS_CODE) {
    goto end;
  }

  // Insert new command
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
  new_command = NULL;
  exit_code = SUCCESS_CODE;
  pouchfile->ncmds++;

end:
  if (new_command != NULL) {
    free(new_command);
  }
  return exit_code;
}

/** Returns whether a certain line starts with the command token. */
static bool is_command(const char* const line, const char* const command) {
  const char* current = line;
  // Skip whitespace
  while (isspace(*current)) ++current;
  // Check if the command is the same.
  const int len = strlen(command);
  return strncmp(current, command, len) == 0 &&
         (isspace(current[len]) || current[len] == '\0');
}

/** Skips the command token and returns a pointer to the argument. */
static char* pouchfile_skip_cmd(char* line, const char* pouchfile_token) {
  char* const end_of_original_line = line + strlen(line);

  char* next_token = strtok_r(line, " \t\n\r\f", NULL);
  if (next_token == NULL) {
    return NULL;
  }

  ASSERT(strcmp(next_token, pouchfile_token) == 0)

  char* to_return = next_token + strlen(next_token);
  // Skip whitespace after token, if there are some.
  while (to_return < end_of_original_line && isspace(*to_return)) {
    ++to_return;
  }
  if (to_return < end_of_original_line) {
    return to_return + 1;
  }

  return to_return;
}

/** Reads a Pouchfile and parses it into a pouchfile struct. */
static pouch_status pouch_build_parse_pouchfile(
    const char* const pouchfile_path, struct pouchfile** const pouchfile) {
  pouch_status exit_code = SUCCESS_CODE;
  char* last_line = NULL;
  int last_line_length = 0;
  int extract_line_status = SUCCESS_CODE;
  bool found_import = false;
  const int pouchfile_fd = open(pouchfile_path, O_RDONLY);
  if (pouchfile_fd < 0) {
    printf(stderr, "Failed to open pouchfile %s\n", pouchfile_path);
    return ERROR_CODE;
  }
  // Allocate pouchfile struct
  if ((exit_code = pouchfile_alloc(pouchfile)) != SUCCESS_CODE) {
    printf(stderr, "Failed to init pouchfile struct\n");
    goto end;
  }

  while (extract_line_status == SUCCESS_CODE) {
    // Read next line
    if ((extract_line_status = next_line(pouchfile_fd, &last_line,
                                         &last_line_length)) == ERROR_CODE) {
      printf(stderr, "Failed to extract run line from Pouchfile\n");
      exit_code = ERROR_CODE;
      goto end;
    }

    if (last_line_length == 0) {
      goto skip_line;
    }

    // Check what command is it.
    bool ok = false;
    for (const struct command_to_type* cmd_to_type = command_to_type_map;
         cmd_to_type->command != NULL; ++cmd_to_type) {
      if (!is_command(last_line, cmd_to_type->command)) {
        continue;
      }
      char* const arg_start =
          pouchfile_skip_cmd(last_line, cmd_to_type->command);
      if (*arg_start == '\0') {
        printf(stderr, "Failed to find argument in first line of Pouchfile\n");
        exit_code = ERROR_CODE;
        goto end;
      }

      // Find parser
      const struct command_type_to_handler* parser = NULL;
      for (parser = command_parsers; parser->command_type != NULL; ++parser) {
        if (parser->command_type == cmd_to_type->type) {
          break;
        }
      }
      ASSERT(parser != NULL);

      if ((exit_code = pouchfile_add_command(*pouchfile, parser, arg_start)) !=
          SUCCESS_CODE) {
        exit_code = ERROR_CODE;
        goto end;
      }

      ok = true;

      // Make sure import is always one and is always the first command.
      if (parser->command_type == POUCHFILE_IMPORT) {
        if (found_import) {
          printf(stderr,
                 "Only one IMPORT command is allowed in the Pouchfile\n");
          exit_code = ERROR_INVALID_IMAGE_NAME_CODE;
          goto end;
        }
        found_import = true;
      } else if (!found_import) {
        printf(stderr,
               "IMPORT command must be the first command in the file\n");
        exit_code = ERROR_INVALID_IMAGE_NAME_CODE;
        goto end;
      }

      break;  // find command
    }

    if (!ok) {
      printf(stderr, "Failed to find command in line: %s\n", last_line);
      exit_code = ERROR_CODE;
      goto end;
    }

  skip_line:
    free(last_line);
  }  // end while file lines
  last_line = NULL;

  // Make sure a valid import was provided.
  if (!found_import) {
    printf(stderr, "Pouchfile must contain an IMPORT command\n");
    exit_code = ERROR_INVALID_IMAGE_NAME_CODE;
  }

end:
  if (last_line != NULL) {
    free(last_line);
  }
  if (exit_code != SUCCESS_CODE) {
    pouch_build_pouchfile_destroy(pouchfile);
  }
  if (pouchfile_fd >= 0) {
    close(pouchfile_fd);
  }
  return exit_code;
}

// === Command executors ===
static pouch_status execute_run_command(
    struct pouchfile_command* const command) {
  printf(stdout, POUCHFILE_RUN_TOKEN " %s\n", command->command);
  // Execute the command.
  POUCH_LOG_DEBUG("Executing command: %s\n", command->command);
  int res = system(command->command);
  POUCH_LOG_DEBUG("Command %s executed with exit code %d\n", command->command,
                  res);
  if (res != 0) {
    printf(stderr, "Failed to execute command %s in container: exit %d\n",
           command->command, res);
    return ERROR_CODE;
  }
  return SUCCESS_CODE;
}

static pouch_status execute_cp_command(
    struct pouchfile_command* const command) {
  printf(stdout, POUCHFILE_COPY_TOKEN " %s\n", command->files_to_copy);
  // Concat the build directory prefix to the source path and copy it to the
  // destination.
  char src_path[MAX_PATH_LENGTH];
  strcpy(src_path, BUILD_BIND_DIR);
  if (*src_path != '/') {
    strcat(src_path, "/");
  }
  strcat(src_path, command->files_to_copy);
  if (cp(src_path, command->files_to_copy) != SUCCESS_CODE) {
    printf(stderr, "Failed to copy files from %s to %s\n", src_path,
           command->files_to_copy);
    return ERROR_CODE;
  }
  return SUCCESS_CODE;
}

struct command_type_to_executor {
  const enum pouchfile_command_type command_type;
  pouch_status (*handler)(struct pouchfile_command* const command);
};

/** A table of commmand executors after the container has started.
 * The handler functions run from the container's context (child process).
 */
static struct command_type_to_executor command_executors[] = {
    {POUCHFILE_RUN, execute_run_command},
    {POUCHFILE_COPY, execute_cp_command},
    {COMMAND_NONE, NULL}};

static pouch_status pouch_build_container_child(
    const struct container_start_config* const config) {
  // 3. Execute all RUN commands in the container.
  struct pouchfile* pouchfile = (struct pouchfile*)config->private_data;
  int n = 1;
  for (struct pouchfile_command* current_command =
           pouchfile->commands_list_head->next;  // Skip IMPORT command.
       current_command != NULL; current_command = current_command->next, ++n) {
    printf(stdout, ">> Step %d/%d: ", n, pouchfile->ncmds - 1);
    // Find executor for command, and run it.
    bool ok = false;
    for (struct command_type_to_executor* executor = command_executors;
         executor->command_type != COMMAND_NONE; ++executor) {
      if (executor->command_type == current_command->type) {
        if (executor->handler(current_command) != SUCCESS_CODE) {
          return ERROR_CODE;
        }
        ok = true;
        break;
      }
    }
    if (!ok) {
      printf(stderr, "Failed to find executor for command #%d\n",
             current_command->type);
      return ERROR_CODE;
    }
  }

  return SUCCESS_CODE;
}

static struct container_mounts_def build_mounts[] = {
    {
        .src = NULL,
        .dest = "",
        .type = IMAGE_ROOT_FS,
    },
    // Add build directory to the container so it can execute COPY commands.
    {
        .src = "/",
        .dest = BUILD_BIND_DIR,
        .type = BIND_MOUNT,
    },
    {
        .src = MUTEX_PREFIX,
        .dest = MUTEX_PREFIX,
        .type = BIND_MOUNT,
    },
    {.type = LIST_END}};

/** Executes the build process for a parsed pouchfile. */
static pouch_status pouch_build_execute(const struct pouchfile* const pouchfile,
                                        const char* const dest_tag) {
  // 1. Copy IMPORTed image to a new destination
  pouch_status status = SUCCESS_CODE;
  ASSERT(pouchfile != NULL &&
         pouchfile->commands_list_head->type == POUCHFILE_IMPORT);
  if ((status = pouch_image_copy(pouchfile->commands_list_head->image_name,
                                 dest_tag)) != SUCCESS_CODE) {
    POUCH_LOG_DEBUG("Failed to copy image %s to %s\n",
                    pouchfile->commands_list_head->image_name, dest_tag);
    return ERROR_CODE;
  }

  struct container_start_config config = {
      .daemonize = false,
      .child_func = pouch_build_container_child,
      .mounts = build_mounts,
      .private_data = (void*)pouchfile,
  };
  strcpy(config.image_name, dest_tag);
  strcpy(config.container_name, dest_tag);
  strcat(config.container_name, BUILD_CONTAINER_NAME_SUFFIX);

  // 2. Start a new container with the copied image
  if ((status = _pouch_container_start(&config)) != SUCCESS_CODE) {
    printf(stderr, "Build failed: failed to start container %s\n",
           config.container_name);
    POUCH_LOG_DEBUG("Failed to start container %s: retruning %d\n",
                    config.container_name, status);
    status = ERROR_CODE;
    goto end;
  }
  POUCH_LOG_DEBUG("Stopping container %s\n", config.container_name);
  status = SUCCESS_CODE;

end:
  if (status != SUCCESS_CODE) {
    if (pouch_image_exists(dest_tag) == SUCCESS_CODE) {
      if (pouch_image_rm(dest_tag) != SUCCESS_CODE) {
        printf(stderr, "Failed to remove image %s\n", dest_tag);
      }
    }
  }

  return status;
}

pouch_status pouch_build(const char* const file_name, const char* const tag) {
  struct pouchfile* pouchfile = NULL;
  pouch_status status = SUCCESS_CODE;
  printf(stderr, "Building pouch image from \"%s\" to tag \"%s\"...\n",
         file_name, tag);

  if ((status = pouch_build_parse_pouchfile(file_name, &pouchfile)) !=
      SUCCESS_CODE) {
    printf(stderr, "Error parsing Pouchfile %s\n", file_name);
    goto end;
  }

  if ((status = pouch_build_execute(pouchfile, tag)) != SUCCESS_CODE) {
    printf(stderr, "Error executing build from Pouchfile %s\n", file_name);
    goto end;
  }

  printf(stderr, "Built image to tag \"%s\".\n", tag);

end:
  pouch_build_pouchfile_destroy(&pouchfile);
  return status;
}
