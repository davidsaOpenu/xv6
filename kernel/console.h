// Console input and output defenitions.
#ifndef XV6_CONSOLE_H
#define XV6_CONSOLE_H

#include "spinlock.h"

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4

// Console driver major number
#define CONSOLE_MAJOR 1
#define CONSOLE_MINOR 0

typedef struct device_lock {
  struct spinlock lock;
  int locking;
} device_lock;

#define INPUT_BUF_SIZE_BYTES 128

#define CMD_HISTORY_SIZE 10

// Append-only circular buffer of commands history
typedef struct {
  char data[CMD_HISTORY_SIZE][INPUT_BUF_SIZE_BYTES];

  // Next index to write at
  int write_idx;

  // Number of entries, cannot decrease
  int count;

  // Persistent cursor for history navigation
  int cursor;
} cmd_history;

typedef struct tty {
  int flags;
  struct spinlock lock;
  uint ttyread_operations_counter;
  uint ttywrite_operations_counter;
  uint tty_bytes_read;
  uint tty_bytes_written;
} tty;

#endif
