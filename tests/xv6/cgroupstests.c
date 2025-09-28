#include "cgroupstests.h"

#include "fcntl.h"
#include "framework/test.h"
#include "kernel/mmu.h"
#include "param.h"
#include "types.h"
#include "user/lib/mutex.h"
#include "user/lib/user.h"

#define GIVE_TURN(my_lock, other_lock)                   \
  do {                                                   \
    ASSERT_EQ(mutex_unlock(&other_lock), MUTEX_SUCCESS); \
    ASSERT_EQ(mutex_lock(&my_lock), MUTEX_SUCCESS)       \
  } while (0)

#define ASSERT_OPEN_CLOSE_READ(x)  \
  ASSERT_TRUE(open_close_file(x)); \
  ASSERT_TRUE(read_file(x, 1))

char controller_names[CONTROLLER_COUNT][MAX_CONTROLLER_NAME_LENGTH] = {
    "cpu", "pid", "set", "mem", "io"};

char suppress = 0;

char temp_path_g[MAX_PATH_LENGTH] = {0};

// ######################################## Helper
//  functions#######################

static int copy_until_char(char* d, char* s, char ch) {
  int len = 0;
  while (*s != ch && *s != '\0') {
    *d++ = *s++;
    len++;
  }

  *d = 0;
  if (*s == ch) len++;

  return len;
}

// Parse memory.stat info and fetch "kernel" value
int get_kernel_total_memory(char* mem_stat_info) {
  char* kernel_value = 0;
  const char KERNEL_NUM_PREFIX[] = "kernel - ";
  kernel_value = strstr(mem_stat_info, (char*)KERNEL_NUM_PREFIX);
  kernel_value += (sizeof(KERNEL_NUM_PREFIX) - 1);
  return atoi(kernel_value);
}

// Return if controller type is valid.
int is_valid_controller_type(int controller_type) {
  return controller_type >= CPU_CNT &&
         controller_type <= CPU_CNT + CONTROLLER_COUNT - 1;
}

// Return controller name.
char* get_controller_name(int controller_type) {
  if (!is_valid_controller_type(controller_type)) {
    return 0;
  }

  return controller_names[controller_type];
}

// Turn on debug messeges or turn them off.
void set_suppress(char _suppress) { suppress = _suppress; }

// Set the given string length to empty.
static void empty_string(char* s, int length) {
  memset(s, 0, length);  // NOLINT(build/include_what_you_use)
}

// Open given file.
int open_file(const char* file) {
  int fd;

  if ((fd = open(file, O_RDWR)) < 1) {
    if (suppress == 0) printf(stdout, "\nFailed to open file: %s\n", file);
    return 0;
  }

  return fd;
}

// Close given file.
int close_file(int fd) {
  if (close(fd) != 0) {
    if (suppress == 0) printf(stdout, "\nFailed to close file\n");
    return 0;
  }

  return 1;
}

// Open and close given file.
int open_close_file(const char* file) {
  int fd = open_file(file);
  if (!fd) {
    return 0;
  }
  return close_file(fd);
}

// Read from a given file. if fails, return 0. Upon success returns the contents
// of the file. If print is set to 1, also prints the returned string.
char* read_file(const char* file, int print) {
  static char buf[256];
  empty_string(buf, 256);

  int fd = open_file(file);
  if (!fd) {
    return 0;
  }
  if (read(fd, buf, 256) < 0) {
    if (suppress == 0) printf(stdout, "\nFailed to read file: %s\n", file);
    close_file(fd);
    return 0;
  }
  char leftover[1];
  if (read(fd, leftover, 1) > 0) {
    printf(stdout, "\nReading of file %s left 1 or more unread bytes\n", file);
    return 0;
  }

  if (print) {
    printf(stdout, "Contents of %s: \n%s\n", file, buf);
  }

  if (!close_file(fd)) {
    return 0;
  }

  return buf;
}

// Write into a file. If succesful returns 1, otherwise 0.
int write_file(const char* file, char* text) {
  char buf[256];
  int fd = open_file(file);

  if (!fd) return 0;
  empty_string(buf, 256);
  strcpy(buf, text);
  if (write(fd, buf, sizeof(buf)) < 0) {
    if (suppress == 0) printf(stdout, "\nFailed to write into file %s\n", file);
    close_file(fd);
    return 0;
  }

  return close_file(fd);
}

// creates a file, returns an fd to the file
int create_file(const char* file) {
  int fd;
  if ((fd = open(file, O_CREATE | O_RDWR)) < 1) {
    if (suppress == 0) printf(stdout, "\nFailed to create a new file \n");
    return 0;
  }
  return fd;
}

int create_and_write_file(const char* file, char* text) {
  int fd;
  if ((fd = create_file(file)) == 0) {
    return 0;
  }

  if (!write_file(file, text)) {
    close_file(fd);
    return 0;
  }

  return fd;
}

// Test enabling controller according to given type.
int enable_controller(int type) {
  char buf[5] = {'+', 0, 0, 0, 0};
  if (!is_valid_controller_type(type)) {
    return 0;
  }

  strcpy(buf + 1, controller_names[type]);

  return write_file(TEST_1_CGROUP_SUBTREE_CONTROL, buf);
}

// Test disabling controller according to given type.
int disable_controller(int type) {
  char buf[5] = {'-', 0};
  if (!is_valid_controller_type(type)) {
    return 0;
  }

  strcpy(buf + 1, controller_names[type]);

  return write_file(TEST_1_CGROUP_SUBTREE_CONTROL, buf);
}

// Test verrifying a controller is active according to given type.
int verify_controller_enabled(int type) {
  char buf[4] = {0};
  if (!is_valid_controller_type(type)) {
    return 0;
  }

  strcpy(buf, controller_names[type]);

  char* contents = read_file(TEST_1_CGROUP_SUBTREE_CONTROL, 0);

  for (int i = 0; i < sizeof(contents) - 2 && contents[i] != 0; i++) {
    if (strcmp("io", buf) == 0) {
      if (contents[i] == 'i' && contents[i + 1] == 'o') {
        return 1;
      }
    }
    if (contents[i] == buf[0] && contents[i + 1] == buf[1] &&
        contents[i + 2] == buf[2]) {
      return 1;
    }
  }

  return 0;
}

// Test verifying a controller is disabled according to given type.
int verify_controller_disabled(int type) {
  char buf[4] = {0};
  if (!is_valid_controller_type(type)) {
    return 0;
  }

  strcpy(buf, controller_names[type]);

  char* contents = read_file(TEST_1_CGROUP_SUBTREE_CONTROL, 0);
  int i;

  for (i = 0; contents[i + 2] != 0; i++) {
    if (contents[i] == buf[0] && contents[i + 1] == buf[1] &&
        contents[i + 2] == buf[2]) {
      printf(stdout, "\nController %s is still enabled\n", buf);
      return 0;
    }
  }

  return 1;
}

// Test moving a process to given cgroup.
int move_proc(const char* file, int pid) {
  char pid_buf[3];
  empty_string(pid_buf, 3);
  itoa(pid_buf, pid);
  return write_file(file, pid_buf);
}

/* Returns buffer of pids (integers) the last entry will be null */
int* get_cgroup_pids(const char* cgroup_proc_file) {
  int* pids = 0;
  char* pids_buffer;
  int i = 0;
  pids_buffer = read_file(cgroup_proc_file, 0);
  if (!pids_buffer) return (int*)0;
  pids = malloc(strlen(pids_buffer) + 4);
  if (pids == 0) return pids;
  while (*pids_buffer != 0) {
    pids[i] = atoi(pids_buffer);
    pids_buffer += 3;
    i++;
  }
  pids[i] = 0;
  return pids;
}

// Test a given pid in string format belongs to a given cgroup.
int is_pid_in_group(const char* file, int pid) {
  char* contents = read_file(file, 0);
  char pid_buf[3];

  empty_string(pid_buf, 3);
  itoa(pid_buf, pid);

  if (pid_buf[1] == 0) pid_buf[1] = '\n';

  int i;
  for (i = 0; contents[i + 1] != 0; i++) {
    if (pid_buf[0] == contents[i] && pid_buf[1] == contents[i + 1]) {
      return 1;
    }
  }

  if (suppress == 0) {
    printf(stdout, "Failed to find pid %d in group %s\n", atoi(pid_buf), file);
  }

  return 0;
}

// Write an integer into a temporary file. Notice only one such file currently
// supported.
int temp_write(int num) {
  int fd;
  if ((fd = open(TEMP_FILE, O_CREATE | O_RDWR)) < 1) {
    if (suppress == 0) printf(stdout, "\nFailed to open a temporary file\n");
    return 0;
  }

  char buf[256];
  itoa(buf, num);
  if (!write_file(TEMP_FILE, buf)) {
    close_file(fd);
    return 0;
  }

  return close_file(fd);
}

// Read an integer from the temporary file.
int temp_read(int print) { return atoi(read_file(TEMP_FILE, print)); }

// Delete the temporary file.
int temp_delete() {
  if (unlink(TEMP_FILE)) {
    printf(stdout, "Failed to delete temporary file\n");
    return 0;
  }

  return 1;
}

// return the value for a given entry from the bufer
// entry mast contains all characters before the value include white-spase
int get_val(char* buf, char* entry) {
  do {
    if (strncmp(buf, entry, strlen(entry)) == 0) {
      buf += strlen(entry);
      return atoi(buf);
    } else {
      /* go to next line. */
      while (*buf++ != '\n') {
      }
    }
  } while (*buf != '\0');
  return -1;  // Assuming all values are supposed to be non-negative
}

// Write into buffer the sequence of activating, disabling then activating a
// given controller. Returns the buffer written.
char* build_activate_disable_activate(int controller_type) {
  if (!is_valid_controller_type(controller_type)) {
    return 0;
  }

  int max_size = (MAX_CONTROLLER_NAME_LENGTH + 2) * 3;
  char* buf = malloc(max_size);
  memset(buf, 0, max_size);

  // "+controller "
  strcat(buf, "+");
  strcat(buf, get_controller_name(controller_type));
  strcat(buf, " ");

  // "-controller "
  strcat(buf, "-");
  strcat(buf, get_controller_name(controller_type));
  strcat(buf, " ");

  // "+controller"
  strcat(buf, "+");
  strcat(buf, get_controller_name(controller_type));

  return buf;
}

// Write into buffer the sequence of activating then disabling a given
// controller. Returns the buffer written.
char* build_activate_disable(int controller_type) {
  if (!is_valid_controller_type(controller_type)) {
    return 0;
  }

  int max_size = (MAX_CONTROLLER_NAME_LENGTH + 2) * 2;
  char* buf = malloc(max_size);
  memset(buf, 0, max_size);

  // "+controller "
  strcat(buf, "+");
  strcat(buf, get_controller_name(controller_type));
  strcat(buf, " ");

  // "-controller"
  strcat(buf, "-");
  strcat(buf, get_controller_name(controller_type));

  return buf;
}

TEST(test_mount_cgroup_fs) {
  ASSERT_FALSE(mkdir(ROOT_CGROUP));
  ASSERT_FALSE(mount(0, ROOT_CGROUP, CGROUP));
}

TEST(test_umount_cgroup_fs) {
  ASSERT_FALSE(umount(ROOT_CGROUP));
  ASSERT_FALSE(unlink(ROOT_CGROUP));
}

TEST(test_creating_cgroups) {
  ASSERT_FALSE(mkdir(TEST_1));
  ASSERT_FALSE(mkdir(TEST_2));
  ASSERT_FALSE(mkdir(TEST_1_1));
  ASSERT_FALSE(mkdir(TEST_1_2));
}

TEST(test_deleting_cgroups) {
  ASSERT_FALSE(unlink(TEST_1_2));
  ASSERT_FALSE(unlink(TEST_1_1));
  ASSERT_FALSE(unlink(TEST_2));
  ASSERT_FALSE(unlink(TEST_1));
}

TEST(test_opening_closing_and_reading_cgroup_files) {
  // Setup
  ASSERT_TRUE(enable_controller(PID_CNT));

  // Tests
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_PROCS);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_CONTROLLERS);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_SUBTREE_CONTROL);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_EVENTS);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_DESCENDANTS);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_MAX_DEPTH);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CGROUP_STAT);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CPU_MAX);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CPU_WEIGHT);
  ASSERT_OPEN_CLOSE_READ(TEST_1_CPU_STAT);
  ASSERT_OPEN_CLOSE_READ(TEST_1_PID_MAX);
  ASSERT_OPEN_CLOSE_READ(TEST_1_PID_CURRENT);
  ASSERT_OPEN_CLOSE_READ(TEST_1_SET_CPU);
  ASSERT_OPEN_CLOSE_READ(TEST_1_SET_FRZ);
  ASSERT_OPEN_CLOSE_READ(TEST_1_MEM_CURRENT);
  ASSERT_OPEN_CLOSE_READ(TEST_1_MEM_MAX);
  ASSERT_OPEN_CLOSE_READ(TEST_1_MEM_MIN);
  ASSERT_OPEN_CLOSE_READ(TEST_1_MEM_STAT);

  // Clean
  ASSERT_TRUE(disable_controller(PID_CNT));
}

TEST(test_opening_and_closing_cgroup_files) {
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_PROCS));
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_CONTROLLERS));
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_SUBTREE_CONTROL));
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_EVENTS));
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_DESCENDANTS));
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_MAX_DEPTH));
  ASSERT_TRUE(open_close_file(TEST_1_CGROUP_STAT));
  ASSERT_TRUE(open_close_file(TEST_1_CPU_MAX));
  ASSERT_TRUE(open_close_file(TEST_1_CPU_WEIGHT));
  ASSERT_TRUE(open_close_file(TEST_1_CPU_STAT));
  ASSERT_TRUE(open_close_file(TEST_1_PID_MAX));
  ASSERT_TRUE(open_close_file(TEST_1_PID_CURRENT));
  ASSERT_TRUE(open_close_file(TEST_1_SET_CPU));
  ASSERT_TRUE(open_close_file(TEST_1_SET_FRZ));
  ASSERT_TRUE(open_close_file(TEST_1_MEM_CURRENT));
  ASSERT_TRUE(open_close_file(TEST_1_MEM_MAX));
  ASSERT_TRUE(open_close_file(TEST_1_MEM_MIN));
  ASSERT_TRUE(open_close_file(TEST_1_MEM_STAT));
  ASSERT_TRUE(open_close_file(TEST_1_MEM_FAILCNT));
}

TEST(test_reading_cgroup_files) {
  ASSERT_TRUE(read_file(TEST_1_CGROUP_PROCS, 1));
  ASSERT_TRUE(read_file(TEST_1_CGROUP_CONTROLLERS, 1));
  ASSERT_TRUE(read_file(TEST_1_CGROUP_SUBTREE_CONTROL, 1));
  ASSERT_TRUE(read_file(TEST_1_CGROUP_EVENTS, 1));
  ASSERT_TRUE(read_file(TEST_1_CGROUP_DESCENDANTS, 1));
  ASSERT_TRUE(read_file(TEST_1_CGROUP_MAX_DEPTH, 1));
  ASSERT_TRUE(read_file(TEST_1_CGROUP_STAT, 1));
  ASSERT_TRUE(read_file(TEST_1_CPU_MAX, 1));
  ASSERT_TRUE(read_file(TEST_1_CPU_WEIGHT, 1));
  ASSERT_TRUE(read_file(TEST_1_CPU_STAT, 1));
  ASSERT_TRUE(read_file(TEST_1_PID_MAX, 1));
  ASSERT_TRUE(read_file(TEST_1_PID_CURRENT, 1));
  ASSERT_TRUE(read_file(TEST_1_SET_CPU, 1));
  ASSERT_TRUE(read_file(TEST_1_SET_FRZ, 1));
  ASSERT_TRUE(read_file(TEST_1_MEM_CURRENT, 1));
  ASSERT_TRUE(read_file(TEST_1_MEM_MAX, 1));
  ASSERT_TRUE(read_file(TEST_1_MEM_MIN, 1));
  ASSERT_TRUE(read_file(TEST_1_MEM_STAT, 1));
  ASSERT_TRUE(read_file(TEST_1_MEM_FAILCNT, 1));
}

int test_enable_and_disable_controller(int controller_type) {
  char* buf;
  int result = 1;

  // Enable given controller.
  buf = build_activate_disable_activate(controller_type);
  if (!buf) {
    return 0;
  }
  result &= write_file(TEST_1_CGROUP_SUBTREE_CONTROL, buf);
  free(buf);

  // Check that the given controller is really enabled.
  result &= verify_controller_enabled(controller_type);

  // Disable the given controller.
  buf = build_activate_disable(controller_type);
  if (!buf) {
    return 0;
  }
  result &= write_file(TEST_1_CGROUP_SUBTREE_CONTROL, buf);
  free(buf);

  // Check that the given controller is really disabled.
  result &= verify_controller_disabled(controller_type);

  return result;
}

TEST(test_enable_and_disable_all_controllers) {
  for (int i = CPU_CNT; i < CPU_CNT + CONTROLLER_COUNT; i++) {
    ASSERT_TRUE(test_enable_and_disable_controller(i));
  }
}

TEST(test_limiting_cpu_max_and_period) {
  // Enable cpu controller
  ASSERT_TRUE(enable_controller(CPU_CNT));

  // Update only max
  ASSERT_TRUE(write_file(TEST_1_CPU_MAX, "5000"));

  // Check changes
  ASSERT_FALSE(
      strcmp(read_file(TEST_1_CPU_MAX, 0), "max - 5000\nperiod - 100000\n"));

  // Update max & period
  ASSERT_TRUE(write_file(TEST_1_CPU_MAX, "1000,20000"));

  // Check changes
  ASSERT_FALSE(
      strcmp(read_file(TEST_1_CPU_MAX, 0), "max - 1000\nperiod - 20000\n"));

  // Disable cpu controller
  ASSERT_TRUE(disable_controller(CPU_CNT));
}

TEST(test_limiting_pids) {
  // Enable pid controller
  ASSERT_TRUE(enable_controller(PID_CNT));

  // Update pid limit
  ASSERT_TRUE(write_file(TEST_1_PID_MAX, "10"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_MAX, 0), "max - 10\n"));

  // Restore to 64
  ASSERT_TRUE(write_file(TEST_1_PID_MAX, "64"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_MAX, 0), "max - 64\n"));
}

TEST(test_move_failure) {
  // Enable pid controller
  ASSERT_TRUE(enable_controller(PID_CNT));

  // Update pid limit
  ASSERT_TRUE(write_file(TEST_1_PID_MAX, "0"));

  // Attempt to move the current process to "/cgroup/test1" cgroup.
  // Notice write here should fail, and so we suppress error outputs from this
  // point.
  ASSERT_FALSE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is not in "/cgroup/test1" cgroup.
  // Notice this should fail.
  ASSERT_FALSE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process is still in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Disable PID controller
  ASSERT_TRUE(disable_controller(PID_CNT));
}

TEST(test_fork_failure) {
  // Enable pid controller
  ASSERT_TRUE(enable_controller(PID_CNT));

  // Update pid limit
  ASSERT_TRUE(write_file(TEST_1_PID_MAX, "1"));

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is really in "/cgroup/test1" cgroup.
  ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Attempt to fork, notice this operation should fail and return -1.
  ASSERT_UINT_EQ(fork(), -1);

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Disable PID controller
  ASSERT_TRUE(disable_controller(PID_CNT));
}

TEST(test_pid_peak) {
  // Ensure pid.peak is reset to 0
  ASSERT_TRUE(disable_controller(PID_CNT));
  ASSERT_TRUE(enable_controller(PID_CNT));
  // Assert initial value of pid.peak is 0
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_PEAK, 0), "0\n"));
  // Move the current process to "/cgroup/test1" cgroup and remove it
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_CURRENT, 0), "1\n"));
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check pid.current is 0 and pid.peak is 1
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_PEAK, 0), "1\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_CURRENT, 0), "0\n"));
  ASSERT_TRUE(disable_controller(PID_CNT));
}

TEST(test_pid_current) {
  ASSERT_TRUE(enable_controller(PID_CNT));
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_CURRENT, 0), "0\n"));
  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is really in "/cgroup/test1" cgroup.
  ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Check pid.current changed to display 1
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_CURRENT, 0), "1\n"));

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Start multiple processes to cgroup and check that pids.current is updated
  // correctly
  int pids[PID_CURRENT_NUMBER_OF_PROCS] = {0};
  for (int i = 0; i < PID_CURRENT_NUMBER_OF_PROCS; i++) {
    int pid = fork();
    if (pid == 0) {
      // Child - run in endless loop
      while (1) sleep(10);
    } else if (pid > 0) {
      // Father
      pids[i] = pid;
      ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, pid));
      int pid_current_val = atoi(read_file(TEST_1_PID_CURRENT, 0));
      ASSERT_EQ(pid_current_val, i + 1);
    } else {
      printf(stdout, "Fork failed in pids.current test\n");
      ASSERT_TRUE(0);
    }
  }

  // Kill the started processes
  int pid_current_val;
  for (int i = 0; i < PID_CURRENT_NUMBER_OF_PROCS; i++) {
    kill(pids[i]);
    wait(NULL);  // Prevent child process become zombie
    pid_current_val = atoi(read_file(TEST_1_PID_CURRENT, 0));
    ASSERT_EQ(pid_current_val, PID_CURRENT_NUMBER_OF_PROCS - 1 - i);
  }

  // Assure pids.current is 0 now
  ASSERT_FALSE(strcmp(read_file(TEST_1_PID_CURRENT, 0), "0\n"));
  ASSERT_TRUE(disable_controller(PID_CNT));
}

TEST(test_moving_process) {
  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is really in "/cgroup/test1" cgroup.
  ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Check that events recorded it correctly.
  ASSERT_FALSE(strcmp(read_file(TEST_1_CGROUP_EVENTS, 0),
                      "populated - 1\nfrozen - 0\n"));

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));
}

TEST(test_setting_max_descendants_and_max_depth) {
  // Set new values for max descendants allowed and max depth allowed
  ASSERT_TRUE(write_file(TEST_1_CGROUP_DESCENDANTS, "30"));
  ASSERT_TRUE(write_file(TEST_1_CGROUP_MAX_DEPTH, "20"));

  // Check that the values have really been set
  ASSERT_FALSE(strcmp(read_file(TEST_1_CGROUP_DESCENDANTS, 0), "30\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_1_CGROUP_MAX_DEPTH, 0), "20\n"));
}

TEST(test_enable_and_disable_set_controller) {
  // Enable cpu set controller.
  ASSERT_TRUE(enable_controller(SET_CNT));

  // Check that cpu set controller is really enabled.
  ASSERT_TRUE(verify_controller_enabled(SET_CNT));

  // Disable cpu set controller.
  ASSERT_TRUE(disable_controller(SET_CNT));

  // Check that cpu set controller is really disabled.
  ASSERT_TRUE(verify_controller_disabled(SET_CNT));
}

TEST(test_setting_cpu_id) {
  // Enable cpu set controller.
  ASSERT_TRUE(enable_controller(SET_CNT));

  // Update cpu id.
  ASSERT_TRUE(write_file(TEST_1_SET_CPU, "1"));

  // Check changes.
  ASSERT_FALSE(strcmp(read_file(TEST_1_SET_CPU, 0), "use_cpu - 1\n"));

  // Restore default cpu id.
  ASSERT_TRUE(write_file(TEST_1_SET_CPU, "0"));

  // Check changes.
  ASSERT_FALSE(strcmp(read_file(TEST_1_SET_CPU, 0), "use_cpu - 0\n"));

  // Disable cpu set controller.
  ASSERT_TRUE(disable_controller(SET_CNT));
}

TEST(test_correct_cpu_running) {
  // Enable cpu set controller.
  ASSERT_TRUE(enable_controller(SET_CNT));

  // Update cpu id.
  ASSERT_TRUE(write_file(TEST_1_SET_CPU, "1"));

  // Check changes.
  ASSERT_FALSE(strcmp(read_file(TEST_1_SET_CPU, 0), "use_cpu - 1\n"));

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is really in "/cgroup/test1" cgroup.
  ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Go to sleep in order to make the process is rescheduled.
  sleep(5);

  // Verify that the process is scheduled on the set cpu.
  ASSERT_UINT_EQ(getcpu(), 1);

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Restore default cpu id.
  ASSERT_TRUE(write_file(TEST_1_SET_CPU, "0"));

  // Check changes.
  ASSERT_FALSE(strcmp(read_file(TEST_1_SET_CPU, 0), "use_cpu - 0\n"));

  // Disable cpu set controller.
  ASSERT_TRUE(disable_controller(SET_CNT));
}

TEST(test_no_run) {
  // Enable cpu set controller.
  ASSERT_TRUE(enable_controller(SET_CNT));

  // Update cpu id.
  ASSERT_TRUE(write_file(TEST_1_SET_CPU, "2"));

  // Fork here since the process should not be running after we move it inside
  // the cgroup.
  int pid = fork();
  int pidToMove = 0;
  int sum = 0;
  int wstatus;

  if (pid == 0) {
    // Child

    pidToMove = getpid();

    // Save the pid of child in temp file.
    temp_write(pidToMove);

    // Go to sleep for long period of time.
    sleep(20);

    // At this point, the child process should already be inside the cgroup.
    // Therefore, the following operations should not be executed right away.
    for (int i = 0; i < 10; i++) {
      sum += 1;
    }

    // Save sum into temp file.
    temp_write(sum);
    exit(0);
  } else {
    // Father

    sleep(5);
    // Read the child pid from temp file.
    pidToMove = temp_read(0);
    // Update the temp file for further reading, since next sum will be read
    // from it.
    ASSERT_TRUE(temp_write(0));

    // Move the child process to "/cgroup/test1" cgroup.
    ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, pidToMove));

    // Check that the process we moved is really in "/cgroup/test1" cgroup.
    ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, pidToMove));

    // Go to sleep to ensure the child process had a chance to be scheduled.
    sleep(10);

    // Verify that the child process have not ran
    sum = temp_read(0);
    ASSERT_UINT_EQ(sum, 0);

    // Return the child to root cgroup.
    ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, pidToMove));

    // Check that the child we returned is really in root cgroup.
    ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, pidToMove));

    // Wait for child to exit.
    wait(&wstatus);
    ASSERT_TRUE(wstatus);

    // Verify that child did execute the procudure.
    sum = temp_read(0);
    ASSERT_UINT_EQ(sum, 10);

    // Remove the temp file.
    ASSERT_TRUE(temp_delete());

    // Disable cpu set controller.
    ASSERT_TRUE(disable_controller(SET_CNT));
  }
}

TEST(test_setting_freeze) {
  // Verify frozen start as 0.
  ASSERT_FALSE(strcmp(read_file(TEST_1_CGROUP_EVENTS, 0),
                      "populated - 0\nfrozen - 0\n"));

  // Update frozen.
  ASSERT_TRUE(write_file(TEST_1_SET_FRZ, "1"));

  // Check changes.
  ASSERT_FALSE(strcmp(read_file(TEST_1_SET_FRZ, 0), "1\n"));

  // Check Evenets correctly recorded.
  ASSERT_FALSE(strcmp(read_file(TEST_1_CGROUP_EVENTS, 0),
                      "populated - 0\nfrozen - 1\n"));

  // Restore frozen.
  ASSERT_TRUE(write_file(TEST_1_SET_FRZ, "0"));

  // Verify frozen is 0 again.
  ASSERT_FALSE(strcmp(read_file(TEST_1_CGROUP_EVENTS, 0),
                      "populated - 0\nfrozen - 0\n"));

  // Check changes.
  ASSERT_FALSE(strcmp(read_file(TEST_1_SET_FRZ, 0), "0\n"));
}

TEST(test_frozen_not_running) {
  // Update frozen.
  ASSERT_TRUE(write_file(TEST_1_SET_FRZ, "1"));

  // Fork here since the process should not be running after we move it inside
  // the cgroup.
  int pid = fork();
  int pidToMove = 0;
  int sum = 0;
  int wstatus;

  if (pid == 0) {
    // Child

    pidToMove = getpid();
    // Save the pid of child in temp file.
    ASSERT_TRUE(temp_write(pidToMove));
    // Go to sleep for long period of time.
    sleep(20);
    // At this point, the child process should already be inside the cgroup,
    // therefore, the following operations should not be executed.
    for (int i = 0; i < 10; i++) {
      sum += 1;
    }
    // Save sum into temp file.
    ASSERT_TRUE(temp_write(sum));
    exit(0);
  } else {
    // Father

    sleep(5);
    // Read the child pid from temp file.
    pidToMove = temp_read(0);
    // Update the temp file for further reading, since next sum will be read
    // from it.
    ASSERT_TRUE(temp_write(0));

    // Move the child process to "/cgroup/test1" cgroup.
    ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, pidToMove));

    // Check that the process we moved is really in "/cgroup/test1" cgroup.
    ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, pidToMove));

    // Go to sleep to ensure the child process had a chance to be scheduled.
    sleep(10);

    // Verify that the child process have not ran
    sum = temp_read(0);
    ASSERT_UINT_EQ(sum, 0);

    // Return the child to root cgroup.
    ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, pidToMove));

    // Check that the child we returned is really in root cgroup.
    ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, pidToMove));

    // Wait for child to exit.
    wait(&wstatus);
    ASSERT_TRUE(wstatus);

    // Verify that child did execute the procudure.
    sum = temp_read(0);
    ASSERT_UINT_EQ(sum, 10);

    // Remove the temp file.
    ASSERT_TRUE(temp_delete());

    // Update frozen.
    ASSERT_TRUE(write_file(TEST_1_SET_FRZ, "0"));
  }
}

TEST(test_mem_peak) {
  // Ensure mem.peak is reset to 0
  ASSERT_TRUE(disable_controller(MEM_CNT));
  ASSERT_TRUE(enable_controller(MEM_CNT));
  // Assert initial value of mem.peak is 0
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_PEAK, 0), "0\n"));
  sbrk(100);

  // Save current process memory size.
  char initial_proc_mem[10];
  strcpy(initial_proc_mem, read_file(TEST_PROC_MEM, 0));
  strcat(initial_proc_mem, "\n");

  // Move the current process to "/cgroup/test1" cgroup and remove it
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_CURRENT, 0), initial_proc_mem));
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_PEAK, 0), initial_proc_mem));

  // Move the process back to root cgroup and resize its memory out of the
  // cgroup
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));
  sbrk(-100);

  // Test peak is preserved and greater than current
  int current_mem = atoi(read_file(TEST_1_MEM_CURRENT, 0));
  int peak_mem = atoi(read_file(TEST_1_MEM_PEAK, 0));
  ASSERT_GT(peak_mem, current_mem);

  // Should reset peak to current after writing to mem.peak
  ASSERT_TRUE(write_file(TEST_1_MEM_PEAK, "1"));

  // Move process back to cgroup and test peak == current again
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));
  current_mem = atoi(read_file(TEST_1_MEM_CURRENT, 0));
  peak_mem = atoi(read_file(TEST_1_MEM_PEAK, 0));
  ASSERT_EQ(peak_mem, current_mem);

  // Clean
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

// In this following memory accounting tests we move only single process to
// "/cgroup/test1" in order to simplify the testing.
TEST(test_mem_current) {
  // Save current process memory size.
  char proc_mem[10];
  strcpy(proc_mem, read_file(TEST_PROC_MEM, 0));
  strcat(proc_mem, "\n");
  // Buffer to read contents from memory file.
  char saved_mem[10];

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is really in "/cgroup/test1" cgroup.
  ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Read the contents of current memory file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_CURRENT, 0));

  // Check memory usaged updated correctly.
  ASSERT_FALSE(strcmp(saved_mem, proc_mem));

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));
}

TEST(test_correct_mem_account_of_growth_and_shrink) {
  // Save current process memory size.
  char proc_mem[10];
  // Buffer to read contents from memory file.
  char saved_mem[10];

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Check that the process we moved is really in "/cgroup/test1" cgroup.
  ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  // Grow the current process by 100 bytes.
  sbrk(100);

  // Read the contents of current memory file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_CURRENT, 0));

  // Convert process memory to a string.
  strcpy(proc_mem, read_file(TEST_PROC_MEM, 0));
  strcat(proc_mem, "\n");

  // Read the contents of current memory file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_CURRENT, 0));

  // Check that the memory accounting correctly updated after memory growth.
  ASSERT_FALSE(strcmp(saved_mem, proc_mem));

  // Decrease current proc by 100 bytes.
  sbrk(-100);

  // Read the contents of current memory file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_CURRENT, 0));

  // Convert process memory to a string.
  strcpy(proc_mem, read_file(TEST_PROC_MEM, 0));
  strcat(proc_mem, "\n");

  // Read the contents of current memory file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_CURRENT, 0));

  // Check that the memory accounting correctly updated after memory growth.
  ASSERT_FALSE(strcmp(saved_mem, proc_mem));

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));
}

TEST(test_limiting_mem) {
  // This test only updates min max values without any verification if the
  // limits apply on the cgroup memory

  // Buffer for saving current memory written in limit
  char default_max[12];
  char default_min[12];

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // read MEM_MAX and MEM_MIN values
  strcpy(default_max, read_file(TEST_1_MEM_MAX, 0));
  strcpy(default_min, read_file(TEST_1_MEM_MIN, 0));

  // Check default limit is correct
  ASSERT_FALSE(strncmp(default_max, KERNBASE, strlen(KERNBASE)));
  ASSERT_FALSE(strncmp(default_min, "0", strlen("0")));

  // Update memory limit
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, "100"));
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, "90"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MAX, 0), "100\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MIN, 0), "90\n"));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Verify limit return to default
  ASSERT_FALSE(strcmp(default_max, read_file(TEST_1_MEM_MAX, 0)));
  ASSERT_FALSE(strcmp(default_min, read_file(TEST_1_MEM_MIN, 0)));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_ensure_mem_min_is_less_then_mem_max) {
  // Mem_max mast to be grater then mem_min

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Update memory max
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, "100"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MAX, 0), "100\n"));

  // Try to update memory min over max this have to fail
  ASSERT_FALSE(write_file(TEST_1_MEM_MIN, "101"));

  // Update memory min
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, "100"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MIN, 0), "100\n"));

  // Try to update memory max smaller then min this have to fail
  ASSERT_FALSE(write_file(TEST_1_MEM_MAX, "99"));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_cant_use_protected_memory) {
  // Try to set mem min for cgroup2 or grows procses sizw
  // These should fail since we protect all memory mem for cgroup1

  // Enable memory controllers
  ASSERT_TRUE(enable_controller(MEM_CNT));
  ASSERT_TRUE(write_file(TEST_2_CGROUP_SUBTREE_CONTROL, "+mem"));

  char buf[12];
  itoa(buf, MEM_SIZE);

  // Protect all memory for cgroup1
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, buf));

  // Check changes
  ASSERT_FALSE(strncmp(read_file(TEST_1_MEM_MIN, 0), buf, strlen(buf)));

  // Try to protect memory for cgroup2 this need to fail
  ASSERT_FALSE(write_file(TEST_2_MEM_MIN, buf));

  // Attempt to grow process memory, notice this operation should fail and
  // return -1.
  ASSERT_UINT_EQ((int)sbrk(MEM_SIZE), -1);

  // Decreas memory min for cgroup1
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, "100"));

  // Update memory min for cgroup2
  ASSERT_TRUE(write_file(TEST_2_MEM_MIN, "100"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_2_MEM_MIN, 0), "100\n"));

  // Restore memory limit to original
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, "0"));
  ASSERT_TRUE(write_file(TEST_2_MEM_MIN, "0"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MIN, 0), "0\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_2_MEM_MIN, 0), "0\n"));

  // Disable memory controllers
  ASSERT_TRUE(disable_controller(MEM_CNT));
  ASSERT_TRUE(write_file(TEST_2_CGROUP_SUBTREE_CONTROL, "-mem"));
}

TEST(test_release_protected_memory_after_delete_cgroup) {
  int i = 0;
  char buf[12] = {0};
  char* mem_str_buf = 0;
  uint kernel_total_mem = 0;
  // We want to reserve different amounts of memory (by precantage)
  float memory_reservations[] = {1.0, 0.75, 0.5, 0.25, 0.1, 0.05, 0.01};

  // Create temp cgroup and enable memory controllers
  for (i = 0; i < sizeof(memory_reservations) / sizeof(float); i++) {
    ASSERT_FALSE(mkdir(TEST_TMP));
    ASSERT_TRUE(enable_controller(MEM_CNT));
    ASSERT_TRUE(write_file(TEST_TMP_CGROUP_SUBTREE_CONTROL, "+mem"));

    // get total amount of memory from memory controller core file
    // (memory.stat)
    mem_str_buf = read_file(TEST_1_MEM_STAT, 0);
    kernel_total_mem = get_kernel_total_memory(mem_str_buf);

    memset(buf, 0, 12);
    itoa(buf, kernel_total_mem * memory_reservations[i]);

    // Protect portion of memory for tmpcgroup
    ASSERT_TRUE(write_file(TEST_TMP_MEM_MIN, buf));

    // Check changes
    ASSERT_FALSE(strncmp(read_file(TEST_TMP_MEM_MIN, 0), buf, strlen(buf)));

    /* Here we change the value we want to reserve to be total kernel's memory
      - X + page_size + 1.
      - Where X is the amount we reserved
      - page_size is the kernel pagesize we use to exceed the amount of
      available space
      - +1 is used in case X is a round value (of 4k page size) so addinf
      PGSIZE won't exceed the available memory space.This is a special case
      where +1 will overflow it for sure.*/
    memset(buf, 0, 12);
    itoa(buf, kernel_total_mem - (kernel_total_mem * memory_reservations[i]) +
                  PGSIZE + 1);

    // Try to protect memory for cgroup1 this need to fail
    ASSERT_FALSE(write_file(TEST_1_MEM_MIN, buf));

    ASSERT_FALSE(unlink(TEST_TMP));
    // Try to protect memory for cgroup1
    ASSERT_TRUE(write_file(TEST_1_MEM_MIN, buf));

    // Disable memory controllers
    ASSERT_TRUE(disable_controller(MEM_CNT));
  }
}

TEST(test_cant_move_under_mem_limit) {
  // Attempt to transfer a process that has allocated MEM_SIZE bytes from one
  // cgroup to another. The attempt should fail since there is no enough
  // memory to protect for the source cgroup.
  char buf[12];
  itoa(buf, MEM_SIZE);

  // Enable memory controllers
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Protect all memory for cgroup1
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, buf));

  // Check changes
  ASSERT_FALSE(strncmp(read_file(TEST_1_MEM_MIN, 0), buf, strlen(buf)));

  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Save current process memory size.
  int proc_mem = atoi(read_file(TEST_PROC_MEM, 0));
  int grow = MEM_SIZE - proc_mem;

  ASSERT_NE((int)sbrk(grow), -1);

  // Try return the process to root cgroup this heve to faile
  ASSERT_FALSE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  ASSERT_NE((int)sbrk(-grow), -1);
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Disable memory controllers
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_mem_limit_negative_and_over_kernelbase) {
  // Buffer for saving current memory written in limit
  char saved_mem[12];

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Copy the current saved memory-max and remove newline at the end
  strcpy(saved_mem, read_file(TEST_1_MEM_MAX, 0));
  saved_mem[strlen(saved_mem) - 1] = '\0';

  // Update memory limit
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, "100"));
  ASSERT_TRUE(write_file(TEST_1_MEM_MIN, "50"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MAX, 0), "100\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MIN, 0), "50\n"));

  // Limit memory by minus
  ASSERT_FALSE(write_file(TEST_1_MEM_MAX, "-100"));
  ASSERT_FALSE(write_file(TEST_1_MEM_MIN, "-100"));

  // Check for no changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MAX, 0), "100\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MIN, 0), "50\n"));

  // Limit memory by over kernel base
  ASSERT_FALSE(write_file(TEST_1_MEM_MAX, MORE_THEN_KERNBASE));
  ASSERT_FALSE(write_file(TEST_1_MEM_MIN, MORE_THEN_KERNBASE));

  // Check for no changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MAX, 0), "100\n"));
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MIN, 0), "50\n"));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_cant_move_over_mem_limit) {
  // Buffer for saving current memory written in limit
  char saved_mem[12];
  char fail_cnt_mem[4];
  uint fail_cnt;

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Copy the current saved memory and remove newline at the end
  strcpy(saved_mem, read_file(TEST_1_MEM_MAX, 0));
  saved_mem[strlen(saved_mem) - 1] = '\0';
  strcpy(fail_cnt_mem, read_file(TEST_1_MEM_FAILCNT, 0));
  fail_cnt_mem[strlen(fail_cnt_mem) - 1] = '\0';

  // Update memory limit
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, "0"));

  // Check changes
  ASSERT_FALSE(strcmp(read_file(TEST_1_MEM_MAX, 0), "0\n"));

  // Attemp to move the current process to "/cgroup/test1" cgroup.
  ASSERT_FALSE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Fail count should be increased by 1 once the process moved to cgroup
  fail_cnt = atoi(fail_cnt_mem) + 1;
  itoa(fail_cnt_mem, fail_cnt);
  strcat(fail_cnt_mem, "\n");

  // Check that the current process is not in "/cgroup/test1" cgroup.
  ASSERT_FALSE(is_pid_in_group(TEST_1_CGROUP_PROCS, getpid()));

  ASSERT_EQ(strcmp(read_file(TEST_1_MEM_FAILCNT, 0), fail_cnt_mem), 0);

  // Check that the current process is still in root group.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Restore memory limit to original
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, saved_mem));

  // Check changes
  ASSERT_FALSE(
      strncmp(read_file(TEST_1_MEM_MAX, 0), saved_mem, strlen(saved_mem)));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_cant_fork_over_mem_limit) {
  // Save current process memory size.
  char proc_mem[10];
  strcpy(proc_mem, read_file(TEST_PROC_MEM, 0));
  // Buffer to read contents from memory file.
  char saved_mem[10];
  char fail_cnt_mem[4];
  uint fail_cnt;

  strcpy(fail_cnt_mem, read_file(TEST_1_MEM_FAILCNT, 0));
  fail_cnt_mem[strlen(fail_cnt_mem) - 1] = '\0';

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Update memory limit
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, proc_mem));

  // Read the contents of limit file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_MAX, 0));

  strcat(proc_mem, "\n");

  // Check changes
  ASSERT_FALSE(strcmp(saved_mem, proc_mem));

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Attempt to fork, notice this operation should fail and return -1.
  ASSERT_UINT_EQ(fork(), -1);

  // Fail count should be increased by 1 once the process moved to cgroup
  fail_cnt = atoi(fail_cnt_mem) + 1;
  itoa(fail_cnt_mem, fail_cnt);
  strcat(fail_cnt_mem, "\n");

  // Fail count should be increased by 1
  ASSERT_EQ(strcmp(read_file(TEST_1_MEM_FAILCNT, 0), fail_cnt_mem), 0);

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_cant_grow_over_mem_limit) {
  // Save current process memory size.
  char proc_mem[10];
  strcpy(proc_mem, read_file(TEST_PROC_MEM, 0));
  // Buffer to read contents from memory file.
  char saved_mem[10];
  char fail_cnt_mem[4];
  uint fail_cnt;

  strcpy(fail_cnt_mem, read_file(TEST_1_MEM_FAILCNT, 0));
  fail_cnt_mem[strlen(fail_cnt_mem) - 1] = '\0';

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Update memory limit
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, proc_mem));

  strcat(proc_mem, "\n");

  // Read the contents of limit file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_MAX, 0));
  strcpy(fail_cnt_mem, read_file(TEST_1_MEM_FAILCNT, 0));

  // Check changes
  ASSERT_FALSE(strcmp(saved_mem, proc_mem));

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Attempt to grow process memory, notice this operation should fail and
  // return -1.
  ASSERT_UINT_EQ((int)sbrk(10), -1);

  // Fail count should be increased by 1 once the process moved to cgroup
  fail_cnt = atoi(fail_cnt_mem) + 1;
  itoa(fail_cnt_mem, fail_cnt);
  strcat(fail_cnt_mem, "\n");

  // Fail count should be increased by 1
  ASSERT_EQ(strcmp(read_file(TEST_1_MEM_FAILCNT, 0), fail_cnt_mem), 0);

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_memory_failcnt_reset) {
  // Save current process memory size.
  char proc_mem[10];
  strcpy(proc_mem, read_file(TEST_PROC_MEM, 0));

  // Buffer to read contents from memory file.
  char saved_mem[10];
  char fail_cnt_mem[4];
  uint fail_cnt;

  strcpy(fail_cnt_mem, read_file(TEST_1_MEM_FAILCNT, 0));
  fail_cnt_mem[strlen(fail_cnt_mem) - 1] = '\0';

  // Enable memory controller
  ASSERT_TRUE(enable_controller(MEM_CNT));

  // Update memory limit
  ASSERT_TRUE(write_file(TEST_1_MEM_MAX, proc_mem));

  strcat(proc_mem, "\n");

  // Read the contents of limit file and convert it for comparison.
  strcpy(saved_mem, read_file(TEST_1_MEM_MAX, 0));
  strcpy(fail_cnt_mem, read_file(TEST_1_MEM_FAILCNT, 0));

  // Check changes
  ASSERT_FALSE(strcmp(saved_mem, proc_mem));

  // Move the current process to "/cgroup/test1" cgroup.
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // Attempt to grow process memory, notice this operation should fail and
  // return -1.
  ASSERT_UINT_EQ((int)sbrk(10), -1);

  // Fail count should be increased by 1 once the process moved to cgroup
  fail_cnt = atoi(fail_cnt_mem) + 1;
  itoa(fail_cnt_mem, fail_cnt);
  strcat(fail_cnt_mem, "\n");

  // Fail count should be increased by 1
  ASSERT_EQ(strcmp(read_file(TEST_1_MEM_FAILCNT, 0), fail_cnt_mem), 0);

  ASSERT_TRUE(write_file(TEST_1_MEM_FAILCNT, "0"));
  ASSERT_EQ(strcmp(read_file(TEST_1_MEM_FAILCNT, 0), "0\n"), 0);

  // Writing any other value than zero is invalid
  ASSERT_FALSE(write_file(TEST_1_MEM_FAILCNT, "1"));

  // Return the process to root cgroup.
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // Check that the process we returned is really in root cgroup.
  ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, getpid()));

  // Disable memory controller
  ASSERT_TRUE(disable_controller(MEM_CNT));
}

TEST(test_memory_stat_content_valid) {
  char buf[265];
  strcpy(buf, read_file(TEST_1_MEM_STAT, 0));
  int file_dirty = get_val(buf, "file_dirty - ");
  int file_dirty_aggregated = get_val(buf, "file_dirty_aggregated - ");
  int pgfault = get_val(buf, "pgfault - ");
  int pgmajfault = get_val(buf, "file_dirty - ");
  ASSERT_UINT_EQ(file_dirty, 0);
  ASSERT_UINT_EQ(file_dirty_aggregated, 0);
  ASSERT_UINT_EQ(pgfault, 0);
  ASSERT_UINT_EQ(pgmajfault, 0);
}

TEST(test_cpu_stat_content_valid) {
  char buf[265];
  strcpy(buf, read_file(TEST_1_CPU_STAT, 0));
  int usage_usec = get_val(buf, "usage_usec - ");
  int user_usec = get_val(buf, "user_usec - ");
  int system_usec = get_val(buf, "system_usec - ");
  ASSERT_UINT_EQ(usage_usec, 0);
  ASSERT_UINT_EQ(user_usec, 0);
  ASSERT_UINT_EQ(system_usec, 0);
}

TEST(test_cpu_stat) {
  char buf1[265];
  char buf2[265];
  char buf3[265];

  // read cpu.stat into a buffer
  strcpy(buf1, read_file(TEST_1_CPU_STAT, 0));

  // Fork here since the process should not be running after we move it inside
  // the cgroup.
  int pid = fork();
  int pidToMove = 0;
  int sum = 0;
  int wstatus;

  if (pid == 0) {
    // Child

    pidToMove = getpid();

    // Save the pid of child in temp file.
    temp_write(pidToMove);

    // Go to sleep for long period of time.
    sleep(10);

    // At this point, the child process should already be inside the cgroup.
    // By running the loop we ensure CPU usage which should be reflected in
    // cpu.stst
    for (int i = 0; i < 10; i++) {
      sum += 1;
    }

    // Save sum into temp file.
    temp_write(sum);

    // Go to sleep to ensure we cen return the child to root cgroup
    sleep(25);
    exit(0);
  } else {
    // Father

    sleep(5);

    // Read the child pid from temp file.
    pidToMove = temp_read(0);

    // Update the temp file for further reading, since next sum will be read
    // from it.
    ASSERT_TRUE(temp_write(0));

    // Move the child process to "/cgroup/test1" cgroup.
    ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, pidToMove));

    // Check that the process we moved is really in "/cgroup/test1" cgroup.
    ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, pidToMove));

    // Go to sleep to ensure the child process had a chance to be scheduled.
    sleep(15);

    // Verify that the child process have ran
    sum = temp_read(0);
    ASSERT_UINT_EQ(sum, 10);

    // Return the child to root cgroup.
    ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, pidToMove));

    // Check that the child we returned is really in root cgroup.
    ASSERT_TRUE(is_pid_in_group(ROOT_CGROUP_PROCS, pidToMove));

    // read cpu.stat into a seconde buffer
    strcpy(buf2, read_file(TEST_1_CPU_STAT, 0));

    // Verify that the cpu time has changed because of the child's runing
    ASSERT_TRUE(strcmp(buf1, buf2));

    sleep(10);

    // read cpu.stat into a third buffer
    strcpy(buf3, read_file(TEST_1_CPU_STAT, 0));

    // Verify that the cpu time has no changed since the child removed
    ASSERT_FALSE(strcmp(buf2, buf3));

    // Wait for child to exit.
    wait(&wstatus);
    ASSERT_TRUE(wstatus);

    // Remove the temp file.
    ASSERT_TRUE(temp_delete());
  }
}

TEST(test_mem_stat) {
  int wstatus;
  char befor_all[265];
  char effect_write_first_file[265];
  char effect_write_second_file[265];
  mutex_t child_mutex;
  mutex_t father_mutex;

  ASSERT_UINT_EQ(mutex_init(&child_mutex), MUTEX_SUCCESS);
  ASSERT_UINT_EQ(mutex_init(&father_mutex), MUTEX_SUCCESS);

  /* The locking mechanism for turn based locking works as following:
   * 1. Both child and father mutex started as locked.
   * 2. When one (child or father) want to wait he locks his lock again.
   * 3. Due to double lock the one who locked is waiting.
   * 4. When one want to wake up the other he unlocks his lock.
   *
   * Therefore when child want to give turn and wait he:
   *   1. mutex_unlock(&father_mutex).
   *   2. mutex_lock(&child_mutex).
   *
   * Created a macro for this mechanism.
   */
  ASSERT_EQ(mutex_lock(&child_mutex), MUTEX_SUCCESS);
  ASSERT_EQ(mutex_lock(&father_mutex), MUTEX_SUCCESS);

  strcpy(befor_all, read_file(TEST_1_MEM_STAT, 0));
  // Fork a process because reading the memory values from inside the cgroup
  // may affect the values.
  int pid = fork();
  int pidToMove = 0;
  if (pid == 0) {
    // Child
    /**** 1 ****/
    pidToMove = getpid();
    // Save the pid of child in temp file.
    ASSERT_TRUE(temp_write(pidToMove));

    // Let father run and wait for child unlock.
    // Allow father to move the process into cgroup.
    GIVE_TURN(child_mutex, father_mutex);

    /**** 3 ****/
    char str[256];
    memset(str, 'a', 256);
    str[255] = '\0'; // BUFFER OVERFLOW MITIGATION

    // Write to a new file 2 times.
    int fd;
    ASSERT_TRUE(fd = create_and_write_file("c", str));
    ASSERT_TRUE(create_and_write_file("c", str));
    ASSERT_TRUE(close_file(fd));

    // Let Father run and wait for child unlock.
    GIVE_TURN(child_mutex, father_mutex);

    /**** 5 ****/
    // Write multiple times to another file with the file closed
    // to create more pgfaults than previous operations
    for (int i = 0; i < 10; i++) {
      ASSERT_TRUE(create_and_write_file("d", str));
      ASSERT_TRUE(close_file(fd));
    }

    // Cleanup
    ASSERT_EQ(mutex_unlock(&father_mutex), MUTEX_SUCCESS);
    ASSERT_EQ(mutex_unlock(&child_mutex), MUTEX_SUCCESS);

    exit(0);
  } else {
    // Father

    // Waits for child to unlock.
    ASSERT_EQ(mutex_lock(&father_mutex), MUTEX_SUCCESS);

    /**** 2 ****/
    // Read the child pid from temp file.
    pidToMove = temp_read(0);
    // Move the child process to "/cgroup/test1" cgroup.
    ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, pidToMove));
    // Check that the process we moved is really in "/cgroup/test1" cgroup.
    ASSERT_TRUE(is_pid_in_group(TEST_1_CGROUP_PROCS, pidToMove));

    // Let child run and wait for father unlock.
    // Allows the child to write a page twice for a new file.
    GIVE_TURN(father_mutex, child_mutex);

    /**** 4 ****/
    strcpy(effect_write_first_file, read_file(TEST_1_MEM_STAT, 0));

    // Let child run and wait for father unlock.
    // Let child run and wait for father unlock.
    // Allows the child to write to a new file close and write again.
    GIVE_TURN(father_mutex, child_mutex);

    /**** 6 ****/
    strcpy(effect_write_second_file, read_file(TEST_1_MEM_STAT, 0));

    // check the effect of pgmajfault
    int pgmajfault_befor = get_val(befor_all, "pgmajfault - ");
    int pgmajfault_after = get_val(effect_write_second_file, "pgmajfault - ");
    ASSERT_GE(pgmajfault_after - pgmajfault_befor, 2);

    // check the effect of pgfault
    // The second write to file c was before closing and file d was after
    // closing, so we need more pgfaults besides what the writing itself
    // causes
    int grow_pgfoult_after_first =
        get_val(effect_write_first_file, "pgfault - ") -
        get_val(befor_all, "pgfault - ");
    int grow_pgfoult_after_seconde =
        get_val(effect_write_second_file, "pgfault - ") -
        get_val(effect_write_first_file, "pgfault - ");
    ASSERT_TRUE(grow_pgfoult_after_first);
    ASSERT_TRUE(grow_pgfoult_after_seconde > grow_pgfoult_after_first);

    // check the effect of file dirty
    // we calculate the dirte and aggregated together as it is impossible to
    // know if there is a delay in writing to disk
    int dirty_befor = get_val(befor_all, "file_dirty - ") +
                      get_val(befor_all, "file_dirty_aggregated - ");
    int dirty_after =
        get_val(effect_write_second_file, "file_dirty - ") +
        get_val(effect_write_second_file, "file_dirty_aggregated - ");
    ASSERT_GE(dirty_after - dirty_befor, 2);
    // Wait for child to exit.
    wait(&wstatus);
    ASSERT_TRUE(wstatus);
    // Remove the temp file.
    ASSERT_TRUE(temp_delete());
  }
}

TEST(test_nested_cgroups) {
  char* mem_str_buf = 0;
  uint kernel_total_mem = 0;
  uint depth_cnt = 1;
  char min_val[12] = {0};
  // char max_val[12] = {0};
  char current_nested_cgroup[MAX_PATH_LENGTH] = {0};
  char current_nesting_index = '0';
  uint current_nested_cgroup_length = 0;

  mem_str_buf = read_file(TEST_1_MEM_STAT, 0);
  kernel_total_mem = get_kernel_total_memory(mem_str_buf);

  printf(stdout, "\nkernel total memory: %x \n", kernel_total_mem);

  // initialize the nested cgroup path
  strcpy(current_nested_cgroup, ROOT_CGROUP);
  strcat(current_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
  current_nested_cgroup[strlen(current_nested_cgroup)] = current_nesting_index;

  /* Create the root nested cgroup and enable the memory controller*/
  ASSERT_FALSE(mkdir(current_nested_cgroup));

  strcpy(temp_path_g, current_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
  ASSERT_TRUE(write_file(temp_path_g, "+mem"));

  /* create the 9 other nested groups. Enable memory controller in each of
     them because it's not propagated from the parent cgroup */
  for (depth_cnt = 1; depth_cnt < 10; depth_cnt++) {
    /* define the min-max values for the current cgroup */
    memset(min_val, 0, 12);
    itoa(min_val, kernel_total_mem / 10);

    // Protect portion of memory for the current nested cgroup
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, current_nested_cgroup);
    strcat(temp_path_g, TEST_NESTED_MEM_MIN);
    printf(stdout, "temp_path_g nested cgroup min path: %s\n", temp_path_g);
    ASSERT_TRUE(write_file(temp_path_g, min_val));
    read_file(temp_path_g, 1);

    // create another nested cgroup (mem controller should be enabled)
    current_nesting_index++;
    strcat(current_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
    current_nested_cgroup[strlen(current_nested_cgroup)] =
        current_nesting_index;
    ASSERT_FALSE(mkdir(current_nested_cgroup));

    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, current_nested_cgroup);
    strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
    ASSERT_TRUE(write_file(temp_path_g, "+mem"));
  }

  // check if we can allocate now more memory in the last cgroup
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, current_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_MEM_MIN);

  // allocate 25% of kernel space - should fail (this should also fail for
  // lesser values)
  memset(min_val, 0, 12);
  itoa(min_val, kernel_total_mem / 4);
  ASSERT_FALSE(write_file(temp_path_g, min_val));

  memset(min_val, 0, 12);
  itoa(min_val, 0);
  current_nested_cgroup_length = strlen(current_nested_cgroup);

  /* disable memory controllers, set min back to 0 and delete cgroups
    Here we do it backwards - reversed tro the last loop */
  for (depth_cnt = 0; depth_cnt < 10; depth_cnt++) {
    // set min value to 0 (just in case)
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, current_nested_cgroup);
    strcat(temp_path_g, TEST_NESTED_MEM_MIN);
    ASSERT_TRUE(write_file(temp_path_g, min_val));

    // disable mem controller
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, current_nested_cgroup);
    strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
    write_file(temp_path_g, "-mem");

    // delete nested cgroup
    ASSERT_FALSE(unlink(current_nested_cgroup));

    current_nested_cgroup_length -= sizeof(TESTED_NESTED_CGROUP_CHILD);
    current_nested_cgroup[current_nested_cgroup_length] = 0;
  }
}

int create_and_move_proc(char* cgroup_path, int mem_allocation) {
  void* allocated_buffer_ptr = NULL;
  int new_proc_pid = fork();

  if (new_proc_pid == 0) {
    // allocate memory to the process
    if (mem_allocation > 0) {
      allocated_buffer_ptr = malloc(mem_allocation);
    }

    // sleep for enough time so the test will finish (2 seconds is a
    // reasonable time)
    usleep(2 * 1000 * 1000);

    // free the allocated bufffer
    free(allocated_buffer_ptr);
    allocated_buffer_ptr = NULL;
    exit(0);
  }

  // move process to the current neset cgroup
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, cgroup_path);
  strcat(temp_path_g, TEST_NESTED_PROCS);

  if (move_proc(temp_path_g, new_proc_pid) <= 0) {
    return 0;
  }

  if (is_pid_in_group(temp_path_g, new_proc_pid) == 0) {
    return 0;
  }

  return new_proc_pid;
}

int reset_nested_memory_controllers(char* top_nested_cgroup_path,
                                    int nesting_level) {
  uint top_nested_cgroup_path_length = 0;
  int depth_cnt = 0;
  char min_value[2] = {'0', 0};
  int* nested_cgroup_procs_pids;
  int i = 0;

  top_nested_cgroup_path_length = strlen(top_nested_cgroup_path);

  /* disable memory controllers, set min back to 0 and delete cgroups
    Here we do it backwards - reversed tro the last loop */
  for (depth_cnt = 0; depth_cnt < nesting_level; depth_cnt++) {
    // set min value to 0 (just in case)
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, top_nested_cgroup_path);
    strcat(temp_path_g, TEST_NESTED_MEM_MIN);
    write_file(temp_path_g, min_value);

    // disable mem controller
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, top_nested_cgroup_path);
    strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
    write_file(temp_path_g, "-mem");

    // kill the processes within the cgroup
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, top_nested_cgroup_path);
    strcat(temp_path_g, TEST_NESTED_PROCS);

    nested_cgroup_procs_pids = get_cgroup_pids(temp_path_g);
    if (nested_cgroup_procs_pids != 0) {
      while (nested_cgroup_procs_pids[i] != 0) {
        kill(nested_cgroup_procs_pids[i]);
        // wait for the child process to exit so it won't be zombie
        wait(0);
        i++;
      }
      i = 0;
      free(nested_cgroup_procs_pids);
    }

    // delete nested cgroup
    if (unlink(top_nested_cgroup_path) < 0) {
      return 0;
    }
    top_nested_cgroup_path_length -= sizeof(TESTED_NESTED_CGROUP_CHILD);
    top_nested_cgroup_path[top_nested_cgroup_path_length] = 0;
  }

  return 1;
}

/* ##################################################################
Scenario 1 - allocate 10 nested cgroups, set min value to 10% of total
kernel's memory and create process in each. Make sure that we can't
allocate more memory than available on each nesting level.
####################################################################*/
void nested_cgroup_mem_recalc_scenario1(int kernel_total_mem, char* min_value) {
  int depth_cnt = 0;
  int current_nesting_index = '0';
  char current_nested_cgroup[MAX_PATH_LENGTH] = {0};
  // maximum available memory to allocate
  char exceeding_mem_value[12] = {0};
  char* name = "nested_cgroup_mem_recalc_scenario1";

  // initialize the nested cgroup path
  strcpy(current_nested_cgroup, ROOT_CGROUP);

  for (depth_cnt = 0; depth_cnt < 10; depth_cnt++) {
    /* Create the root nested cgroup and enable the memory controller*/
    strcat(current_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
    current_nested_cgroup[strlen(current_nested_cgroup)] =
        current_nesting_index;
    ASSERT_FALSE(mkdir(current_nested_cgroup));
    strcpy(temp_path_g, current_nested_cgroup);
    strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
    ASSERT_TRUE(write_file(temp_path_g, "+mem"));

    // Protect portion of memory for the current nested cgroup
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, current_nested_cgroup);
    strcat(temp_path_g, TEST_NESTED_MEM_MIN);
    printf(stdout, "temp_path_g nested cgroup min path: %s\n", temp_path_g);

    /* first, try to allocate the free memory in the kernel + 1 PAGE. This
     * should fail */
    itoa(exceeding_mem_value,
         atoi(min_value) * (NESTED_CGROUPS_LEVEL + 1 - depth_cnt) + PGSIZE);
    ASSERT_FALSE(write_file(temp_path_g, exceeding_mem_value));

    /* set the min value to the cgroup */
    ASSERT_TRUE(write_file(temp_path_g, min_value));
    // read_file(temp_path_g, 1);

    // create process to the the cgroup
    ASSERT_TRUE(create_and_move_proc(current_nested_cgroup, 0));

    current_nesting_index++;
  }

  // current_nested_cgroup should be the path to the top (last) created nested
  // cgroup
  ASSERT_TRUE(reset_nested_memory_controllers(current_nested_cgroup,
                                              NESTED_CGROUPS_LEVEL));
}

/* ##################################################################
Scenario 2 - allocate 10 nested cgroups, set min value from the top
nested cgroup through the bottom (root cgroup) and create process in
each layer.
On each layer, make sure that we can't allocate more than available
memory (after the cgroup in the layer above set min value).
####################################################################*/
void nested_cgroup_mem_recalc_scenario2(int kernel_total_mem, char* min_value) {
  int depth_cnt = 0;
  int current_nesting_index = '0';
  char current_nested_cgroup[MAX_PATH_LENGTH] = {0};
  char descending_nested_cgroup_path[MAX_PATH_LENGTH] = {0};
  char exceeding_memory_value[12] = {0};
  char* name = "nested_cgroup_mem_recalc_scenario2";
  int current_path_length = 0;

  // initialize the nested cgroup path
  strcpy(current_nested_cgroup, ROOT_CGROUP);

  for (depth_cnt = 0; depth_cnt < 10; depth_cnt++) {
    /* Create the nested cgroup and enable the memory controller*/
    strcat(current_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
    current_nested_cgroup[strlen(current_nested_cgroup)] =
        current_nesting_index;
    ASSERT_FALSE(mkdir(current_nested_cgroup));
    strcpy(temp_path_g, current_nested_cgroup);

    printf(stdout, "temp_path_g nested cgroup path: %s\n", temp_path_g);

    strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
    ASSERT_TRUE(write_file(temp_path_g, "+mem"));

    current_nesting_index++;
  }

  strcpy(descending_nested_cgroup_path, current_nested_cgroup);
  current_path_length = strlen(descending_nested_cgroup_path);

  /* Try the allocations tests on the created cgroups*/
  for (depth_cnt = 10; depth_cnt > 0; depth_cnt--) {
    memset(temp_path_g, 0, MAX_PATH_LENGTH);
    strcpy(temp_path_g, descending_nested_cgroup_path);
    strcat(temp_path_g, TEST_NESTED_MEM_MIN);
    printf(stdout, "temp_path_g nested cgroup min path: %s\n", temp_path_g);

    /* first, try to allocate the free memory in the kernel + 1 PAGE. This
     * should fail */
    itoa(exceeding_memory_value, atoi(min_value) * (depth_cnt + 1) + PGSIZE);
    ASSERT_FALSE(write_file(temp_path_g, exceeding_memory_value));

    /* allocate only 10% of memory now */
    ASSERT_TRUE(write_file(temp_path_g, min_value));

    current_path_length -= sizeof(TESTED_NESTED_CGROUP_CHILD);
    descending_nested_cgroup_path[current_path_length] = 0;
  }

  // current_nested_cgroup should be the path to the top (last) created nested
  // cgroup
  ASSERT_TRUE(reset_nested_memory_controllers(current_nested_cgroup,
                                              NESTED_CGROUPS_LEVEL));
}

/* ##################################################################
Scenario 3 - set maximum memory threshold on the root cgroup.
Then, we create a nested cgroup and try to allocate above the threshold.
####################################################################*/
void nested_cgroup_mem_recalc_scenario3(int kernel_total_memory) {
  char current_nested_cgroup[MAX_PATH_LENGTH] = {0};
  char max_mem_threshold[12] = {0};
  char* name = "nested_cgroup_mem_recalc_scenario3";

  // initialize the nested cgroup path and enable memory controller
  strcpy(current_nested_cgroup, ROOT_CGROUP);

  // create nested cgroup and set the maximum threshold
  strcat(current_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
  current_nested_cgroup[strlen(current_nested_cgroup)] = '0';
  ASSERT_FALSE(mkdir(current_nested_cgroup));
  strcpy(temp_path_g, current_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
  ASSERT_TRUE(write_file(temp_path_g, "+mem"));

  // set the maximum threshold  (which is half the total kernel's memory)
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, current_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_MEM_MAX);

  itoa(max_mem_threshold, kernel_total_memory / 2);
  ASSERT_TRUE(write_file(temp_path_g, max_mem_threshold));
  read_file(temp_path_g, 1);

  // create another nested cgroup and try to allocate more than the threshold
  strcat(current_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
  current_nested_cgroup[strlen(current_nested_cgroup)] = '1';
  ASSERT_FALSE(mkdir(current_nested_cgroup));
  strcpy(temp_path_g, current_nested_cgroup);

  strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
  ASSERT_TRUE(write_file(temp_path_g, "+mem"));

  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, current_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_MEM_MIN);
  memset(max_mem_threshold, 0, sizeof(max_mem_threshold));
  itoa(max_mem_threshold, (kernel_total_memory / 2) + PGSIZE);

  ASSERT_FALSE(write_file(temp_path_g, max_mem_threshold));

  // make sure we still can allocate below the threshold
  memset(max_mem_threshold, 0, sizeof(max_mem_threshold));
  itoa(max_mem_threshold, (kernel_total_memory / 2) - PGSIZE);
  ASSERT_TRUE(write_file(temp_path_g, max_mem_threshold));

  ASSERT_TRUE(reset_nested_memory_controllers(current_nested_cgroup, 2));
}

/* ##################################################################
Scenario 4 - Move process from nested cgroup to its parent and
back. The parent and the child cgroup should set their maximum
memory to PGSIZE (4096 bytes).
####################################################################*/
void nested_cgroup_mem_recalc_scenario4(int kernel_total_memory) {
  char parent_nested_cgroup[MAX_PATH_LENGTH] = {0};
  char child_nested_cgroup[MAX_PATH_LENGTH] = {0};
  char max_mem_threshold[12] = {0};
  char* name = "nested_cgroup_mem_recalc_scenario4";
  int pid = -1;

  // initialize the nested cgroup path and enable memory controller
  strcpy(parent_nested_cgroup, ROOT_CGROUP);

  // create nested cgroup and set the maximum threshold
  strcat(parent_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
  parent_nested_cgroup[strlen(parent_nested_cgroup)] = '0';
  ASSERT_FALSE(mkdir(parent_nested_cgroup));
  strcpy(temp_path_g, parent_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
  ASSERT_TRUE(write_file(temp_path_g, "+mem"));

  // set the maximum threshold  (which is half the total kernel's memory)
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, parent_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_MEM_MAX);

  itoa(max_mem_threshold, PGSIZE * 100);
  ASSERT_TRUE(write_file(temp_path_g, max_mem_threshold));
  read_file(temp_path_g, 1);

  // create the process with PGSIZE bytes allocation
  //  This should fail the process memory is much bigger
  ASSERT_TRUE(pid = create_and_move_proc(parent_nested_cgroup, PGSIZE));

  // create the nested cgroup (its max value should be as its parent max
  // value)
  strcpy(child_nested_cgroup, parent_nested_cgroup);
  strcat(child_nested_cgroup, TESTED_NESTED_CGROUP_CHILD);
  child_nested_cgroup[strlen(child_nested_cgroup)] = '1';
  ASSERT_FALSE(mkdir(child_nested_cgroup));
  strcpy(temp_path_g, child_nested_cgroup);

  strcat(temp_path_g, TEST_NESTED_SUBTREE_CONTROL);
  ASSERT_TRUE(write_file(temp_path_g, "+mem"));

  // move the process to the child cgroup
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, child_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_PROCS);
  ASSERT_TRUE(move_proc(temp_path_g, pid));
  // make sure process not in parent
  ASSERT_TRUE(is_pid_in_group(temp_path_g, pid));

  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, parent_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_PROCS);
  ASSERT_FALSE(is_pid_in_group(temp_path_g, pid));

  // move proces back to parent
  ASSERT_TRUE(move_proc(temp_path_g, pid));
  ASSERT_TRUE(is_pid_in_group(temp_path_g, pid));
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, child_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_PROCS);
  ASSERT_FALSE(is_pid_in_group(child_nested_cgroup, pid));

  // set max memory of child to only 10*PGSIZE bytes
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, child_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_MEM_MAX);

  itoa(max_mem_threshold, PGSIZE * 10);
  ASSERT_TRUE(write_file(temp_path_g, max_mem_threshold));
  read_file(temp_path_g, 1);

  // move process to child again (this should fail)
  memset(temp_path_g, 0, MAX_PATH_LENGTH);
  strcpy(temp_path_g, child_nested_cgroup);
  strcat(temp_path_g, TEST_NESTED_PROCS);
  ASSERT_FALSE(move_proc(temp_path_g, pid));

  ASSERT_TRUE(reset_nested_memory_controllers(child_nested_cgroup, 2));
}

TEST(test_nested_cgroup_memory_recalculation) {
  int kernel_total_mem = 0;
  char* mem_stat_buf = 0;
  char min_value[12] = {0};

  mem_stat_buf = read_file(TEST_1_MEM_STAT, 0);
  kernel_total_mem = get_kernel_total_memory(mem_stat_buf);

  /* define the min-max values for the current cgroup . We divide in 11
    because we want to make sure that all of the 10 nested levels
    will fit in the total kernel's memory
  */
  itoa(min_value, kernel_total_mem / (NESTED_CGROUPS_LEVEL + 1));
  printf(stdout, "\nThe min_value for allocation is: %s\n", min_value);

  nested_cgroup_mem_recalc_scenario1(kernel_total_mem, min_value);
  sleep(1);

  nested_cgroup_mem_recalc_scenario2(kernel_total_mem, min_value);
  sleep(1);

  nested_cgroup_mem_recalc_scenario3(kernel_total_mem);
  sleep(1);

  nested_cgroup_mem_recalc_scenario4(kernel_total_mem);
}

TEST(test_io_stat_content_valid) {
  ASSERT_FALSE(strcmp(read_file(TEST_1_IO_STAT, 0), "\n"));
}

struct io_stat_line {
  int major, minor, rbytes, wbytes, rios, wios, dbytes, dios;
};

void parse_io_stat_line(struct io_stat_line* stat, char* line) {
  static char tmp_buffer[128];

  int len = copy_until_char(tmp_buffer, line, ':');
  stat->major = atoi(tmp_buffer);
  line += len;

  len = copy_until_char(tmp_buffer, line, '\t');
  stat->minor = atoi(tmp_buffer);
  line += len;

  line += 7;  // skip rbytes=
  len = copy_until_char(tmp_buffer, line, '\t');
  stat->rbytes = atoi(tmp_buffer);
  line += len;

  line += 7;  // skip wbytes=
  len = copy_until_char(tmp_buffer, line, '\t');
  stat->wbytes = atoi(tmp_buffer);
  line += len;

  line += 5;  // skip rios=
  len = copy_until_char(tmp_buffer, line, '\t');
  stat->rios = atoi(tmp_buffer);
  line += len;

  line += 5;  // skip wios=
  len = copy_until_char(tmp_buffer, line, '\t');
  stat->wios = atoi(tmp_buffer);
  line += len;

  line += 7;  // skip dbytes=
  len = copy_until_char(tmp_buffer, line, '\t');
  stat->dbytes = atoi(tmp_buffer);
  line += len;

  line += 5;  // skip dios=
  len = copy_until_char(tmp_buffer, line, '\n');
  stat->dios = atoi(tmp_buffer);
}

// return -1 on faiulr
int parse_io_stat_file(struct io_stat_line table[], int max_table_size) {
  static char buf[1024];

  char* file_content = read_file(TEST_1_IO_STAT, 0);
  if (!file_content) {
    return -1;
  }
  strcpy(buf, file_content);

  char* s = buf;
  int i = 0;

  for (i = 0; i < max_table_size; i++) {
    if (*s == '\n' || *s == '\0') {
      break;
    }
    parse_io_stat_line(&table[i], s);
    while (*s != '\n') s++;  // move to the next line
    if (*s != '\0') s++;
  }
  return i;
}

// NOTE: open and close make io from the disk for reading/writing fs data
// (directory entries for example) therefor we use open and close outside of the
// cgroup.
/**
 * Write and read 10 bytes from file then expect that compared to former value
 * rbytes is +10 , wbytes +10 rio+1, wio+1 Write and read 10 bytes from screen
 * then expect that compared to former value rbytes is +10 , wbytes +10 rio+1,
 * wio+1
 */
TEST(test_io_stat) {
  static const int STATE_TABLE_MAX_SIZE = 5;
  static const int NUM_BYTES_TO_TEST = 10;

  struct io_stat_line stat_table_before[STATE_TABLE_MAX_SIZE];
  int before_table_size =
      parse_io_stat_file(stat_table_before, STATE_TABLE_MAX_SIZE);
  ASSERT_NE(before_table_size, -1);

  int fd = open(TEMP_FILE, O_CREATE | O_RDWR);

  char str_to_print[NUM_BYTES_TO_TEST + 1];
  memset(str_to_print, 'A', NUM_BYTES_TO_TEST);
  str_to_print[NUM_BYTES_TO_TEST] = 0;

  // go into TEST1 cgroup to print and write to file
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  // write exactly NUM_BYTES_TO_TEST bytes to screen
  printf(stdout, "%s", str_to_print);

  // write and red NUM_BYTES_TO_READ_WRITE bytes to file

  ASSERT_TRUE(write(fd, str_to_print, NUM_BYTES_TO_TEST) == NUM_BYTES_TO_TEST);

  // get out of TEST1 cgroup
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  // for getting the file offeset to the beginning of the file we need to close
  // and open it and we can't close and open inside the cgroup because
  // open/close use io.
  close(fd);
  fd = open(TEMP_FILE, O_RDONLY);

  // go back into TEST1 cgroup
  ASSERT_TRUE(move_proc(TEST_1_CGROUP_PROCS, getpid()));

  char file_content[NUM_BYTES_TO_TEST * 2];
  empty_string(file_content, sizeof(file_content));
  // read the file intp file_content -- should read 10 exactly bytes
  read(fd, file_content, NUM_BYTES_TO_TEST * 2);
  ASSERT_FALSE(strcmp(file_content, str_to_print));

  // get out of TEST1 cgroup
  ASSERT_TRUE(move_proc(ROOT_CGROUP_PROCS, getpid()));

  close(fd);
  ASSERT_TRUE(temp_delete());

  // clean the console
  memset(str_to_print, '\b', NUM_BYTES_TO_TEST);
  printf(stdout, "%s", str_to_print);
  memset(str_to_print, ' ', NUM_BYTES_TO_TEST);
  printf(stdout, "%s", str_to_print);
  memset(str_to_print, '\b', NUM_BYTES_TO_TEST);
  printf(stdout, "%s", str_to_print);

  // parse io.stat into stat_table_after
  struct io_stat_line stat_table_after[STATE_TABLE_MAX_SIZE];
  int after_table_size =
      parse_io_stat_file(stat_table_after, STATE_TABLE_MAX_SIZE);
  ASSERT_NE(after_table_size, -1);

  struct io_stat_line* disk_before = 0;
  struct io_stat_line* disk_after = 0;
  struct io_stat_line* screen_before = 0;
  struct io_stat_line* screen_after = 0;

  // find the disk and screen entries
  for (int i = 0; i < after_table_size; i++) {
    if (stat_table_after[i].major == 0 && stat_table_after[i].minor == 0) {
      disk_after = &stat_table_after[i];
    }
    if (stat_table_after[i].major == 1 && stat_table_after[i].minor == 0) {
      screen_after = &stat_table_after[i];
    }
  }
  for (int i = 0; i < before_table_size; i++) {
    if (stat_table_before[i].major == 0 && stat_table_before[i].minor == 0) {
      disk_before = &stat_table_before[i];
    }
    if (stat_table_before[i].major == 1 && stat_table_before[i].minor == 0) {
      screen_before = &stat_table_before[i];
    }
  }

  // read and wrote NUM_BYTES_TO_TEST bytes in disk in 1 read and 1 write
  if (disk_before) {
    ASSERT_UINT_EQ(disk_before->rbytes + NUM_BYTES_TO_TEST, disk_after->rbytes);
    ASSERT_UINT_EQ(disk_before->wbytes + NUM_BYTES_TO_TEST, disk_after->wbytes);
    ASSERT_UINT_EQ(disk_before->rios + 1, disk_after->rios);
    ASSERT_UINT_EQ(disk_before->wios + 1, disk_after->wios);
  } else {
    ASSERT_UINT_EQ(NUM_BYTES_TO_TEST, disk_after->rbytes);
    ASSERT_UINT_EQ(NUM_BYTES_TO_TEST, disk_after->wbytes);
    ASSERT_UINT_EQ(1, disk_after->rios);
    ASSERT_UINT_EQ(1, disk_after->wios);
  }
  // read 0 bytes from screen in 0 reads and wrote NUM_BYTES_TO_TEST bytes in
  // NUM_BYTES_TO_TEST writes (printf calls for putc for each char)
  if (screen_before) {
    ASSERT_UINT_EQ(screen_before->rbytes, screen_after->rbytes);
    ASSERT_UINT_EQ(screen_before->wbytes + NUM_BYTES_TO_TEST,
                   screen_after->wbytes);
    ASSERT_UINT_EQ(screen_before->rios, screen_after->rios);
    ASSERT_UINT_EQ(screen_before->rios + NUM_BYTES_TO_TEST, screen_after->wios);

  } else {
    ASSERT_UINT_EQ(0, screen_after->rbytes);
    ASSERT_UINT_EQ(NUM_BYTES_TO_TEST, screen_after->wbytes);
    ASSERT_UINT_EQ(0, screen_after->rios);
    ASSERT_UINT_EQ(1, screen_after->wios);
  }
  ASSERT_EQ(screen_after->dbytes, 0);
  ASSERT_EQ(screen_after->dios, 0);
  ASSERT_EQ(disk_after->dbytes, 0);
  ASSERT_EQ(disk_after->dios, 0);
}

INIT_TESTS_PLATFORM();

int main(int argc, char* argv[]) {
  // comment out for debug messages
  set_suppress(1);

  run_test(test_mount_cgroup_fs);
  run_test(test_creating_cgroups);
  run_test(test_opening_closing_and_reading_cgroup_files);
  run_test(test_memory_stat_content_valid);
  run_test(test_cpu_stat_content_valid);
  run_test(test_io_stat_content_valid);
  run_test(test_moving_process);
  run_test(test_enable_and_disable_all_controllers);
  run_test(test_limiting_pids);
  run_test(test_move_failure);
  run_test(test_io_stat);
  run_test(test_fork_failure);
  run_test(test_cpu_stat);
  run_test(test_pid_peak);
  run_test(test_pid_current);
  run_test(test_setting_cpu_id);
  // run_test(test_correct_cpu_running);
  run_test(test_no_run);
  run_test(test_mem_stat);
  run_test(test_setting_freeze);
  run_test(test_frozen_not_running);
  run_test(test_mem_peak);
  run_test(test_mem_current);
  run_test(test_correct_mem_account_of_growth_and_shrink);
  run_test(test_limiting_mem);
  run_test(test_ensure_mem_min_is_less_then_mem_max);
  run_test(test_cant_use_protected_memory);
  run_test(test_release_protected_memory_after_delete_cgroup);
  run_test(test_cant_move_under_mem_limit);
  run_test(test_nested_cgroups);
  run_test(test_nested_cgroup_memory_recalculation);
  run_test(test_mem_limit_negative_and_over_kernelbase);
  run_test(test_cant_move_over_mem_limit);
  run_test(test_cant_fork_over_mem_limit);
  run_test(test_cant_grow_over_mem_limit);
  run_test(test_memory_failcnt_reset);
  run_test(test_limiting_cpu_max_and_period);
  run_test(test_setting_max_descendants_and_max_depth);
  run_test(test_deleting_cgroups);
  run_test(test_umount_cgroup_fs);

  PRINT_TESTS_RESULT("CGROUPTESTS");
  return CURRENT_TESTS_RESULT();
}
