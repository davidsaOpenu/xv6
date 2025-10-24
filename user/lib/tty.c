#include "fcntl.h"
#include "stat.h"
#include "types.h"
#include "user.h"
#include "x86.h"

int is_attached_tty(int tty_fd) { return ioctl(tty_fd, TTYGETS, DEV_ATTACH); }

int is_connected_tty(int tty_fd) { return ioctl(tty_fd, TTYGETS, DEV_CONNECT); }

int attach_tty(int tty_fd) { return attach_tty_pid(tty_fd, -1); }

int detach_tty(int tty_fd) { return detach_tty_pid(tty_fd, -1); }

int attach_tty_pid(int tty_fd, int pid) {
  if (ioctl(tty_fd, TTYSETS, DEV_ATTACH, pid) < 0) return -1;
  return 0;
}

int reset_std_fds(int tty_fd) {
  close(0);
  close(1);
  close(2);

  if (dup(tty_fd) < 0) return -1;
  if (dup(tty_fd) < 0) return -1;
  if (dup(tty_fd) < 0) return -1;

  return 0;
}

int detach_tty_pid(int tty_fd, int pid) {
  ioctl(tty_fd, TTYSETS, DEV_DETACH, pid);
  return 0;
}

int connect_tty(int tty_fd) {
  if (!is_attached_tty(tty_fd)) {
    close(tty_fd);
    return -1;
  }

  ioctl(tty_fd, TTYSETS, DEV_CONNECT);
  return tty_fd;
}

int disconnect_tty(int tty_fd) {
  return ioctl(tty_fd, TTYSETS, DEV_DISCONNECT);
}
