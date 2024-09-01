/*
 *   Pouch.h
 *   - pouch container commands
 */
#ifndef XV6_POUCH_H
#define XV6_POUCH_H

#define POUCH_CGROUPS_DIR "/cgroup"

/**
 * Pouch CLI exit codes.
 */
enum pouch_exit_code {
  POUCH_EXIT_SUCCESS = 0,
  POUCH_EXIT_FAILURE = 1,
};

/**
 * Pouch CLI commands.
 */
typedef enum p_cmd {
  P_CMD_INVALID = 0,
  P_CMD_START,
  P_CMD_CONNECT,
  P_CMD_DISCONNECT,
  P_CMD_DESTROY,
  P_CMD_LIMIT,
  P_CMD_INFO_OUTSIDE,
  P_CMD_INFO_INSIDE,
  P_CMD_LIST,
  P_CMD_IMAGES,
  P_CMD_BUILD,
  P_CMD_HELP,

  P_CMD_MAX  // for array size
} p_cmd;

/**
 * Pouch internal status codes.
 */
typedef enum POUCH_INTERNAL_STATUS_CODES {
  SUCCESS_CODE = 0,
  END_OF_FILE_CODE = 1,
  // Error codes < 0
  ERROR_CODE = -1,
  ERROR_IMAGE_NAME_TOO_LONG_CODE = -2,
  ERROR_IMAGE_NOT_FOUND_CODE = -3,
  ERROR_OUT_OF_MEMORY_CODE = -4,
  ERROR_INVALID_IMAGE_NAME_CODE = -5,
  ERROR_IMAGE_DIR_INVALID_CODE = -6,
  ERROR_IMAGE_INVALID_CODE = -7,
  CONTAINER_NAME_TOO_LONG_CODE = -8,
  TTY_CLOSE_ERROR_CODE = -9,
  TTY_DISCONNECT_ERROR_CODE = -10,
  TTY_DETACH_ERROR_CODE = -11,
  TTY_OPEN_ERROR_CODE = -12,
  TTY_CONNECT_ERROR_CODE = -13,
  KILL_CONTAINER_PROC_ERROR_CODE = -14,
  CONTAINER_NOT_FOUND_CODE = -15,
  POUCH_MUTEX_ERROR_CODE = -16,
  UNSAHRE_MNT_NS_FAILED_ERROR_CODE = -17,
  IMAGE_MOUNT_FAILED_ERROR_CODE = -18,
  CONTAINER_ALREADY_STARTED_CODE = -19,
  UNSAHRE_PID_NS_FAILED_ERROR_CODE = -20,
  POUCH_FORK_FAILED_ERROR_CODE = -21,
  CGROUP_CREATE_FAILED_CODE = -22,
  MOUNT_CGROUP_FAILED_ERROR_CODE = -23,
  NO_AVAILABLE_TTY_ERROR_CODE = -24,
  INVALID_CCONF_ERROR_CODE = -25,
  INVALID_CCONF_TO_WRITE_ERROR_CODE = -26,
  FAILED_TO_OPEN_CCONF_ERROR_CODE = -27,
  FAILED_TO_CLOSE_CCONF_ERROR_CODE = -28,
} pouch_status;

/**
 * Flags to describe command validity inside or outside a container.
 */
enum cmd_spec_inside_or_out {
  INSIDE_CONTAINER = 1,
  OUTSIDE_CONTAINER = 2,
  INSIDE_AND_OUTSIDE_CONTAINER = INSIDE_CONTAINER | OUTSIDE_CONTAINER
};

/**
 * Describes a pouch command.
 * Each command is defined by the p_cmd enum value,
 * recognized command name (for CLI), callback function (to handle execution),
 * and whether the command is valid inside or outside a container (or both).
 */
struct pouch_cli_command {
  const p_cmd cmd;
  pouch_status (*callback)(const p_cmd cmd, const int argc,
                           const char* const argv[]);
  const char* command_name;
  enum cmd_spec_inside_or_out inside_or_out;
};

#endif /* XV6_POUCH_H */
