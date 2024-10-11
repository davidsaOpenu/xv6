#include "lib/user.h"
#include "types.h"

int main(int argc, char *argv[]) {
  int microseconds = 0;

  if (argc != 2) {
    printf(stderr, "usage: %s [sleep_microseconds]\n", argv[0]);
    exit(1);
  }

  microseconds = atoi(argv[1]);

  while (1) {
    printf(stdout, "cpu time: %d, cpu percent: %d\n",
           ioctl(0, IOCTL_GET_PROCESS_CPU_TIME, 0),
           ioctl(0, IOCTL_GET_PROCESS_CPU_PERCENT, 0));
    if (microseconds) {
      usleep(microseconds);
    }
  }
  return 0;
}
