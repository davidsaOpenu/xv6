// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "console.h"

#include "defs.h"
#include "fcntl.h"
#include "file.h"
#include "fs.h"
#include "kvector.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "pid_ns.h"
#include "proc.h"
#include "sleeplock.h"
#include "traps.h"
#include "types.h"
#include "vfs_file.h"
#include "x86.h"

static ushort *crt = (ushort *)P2V(0xb8000);  // CGA memory
static int panicked = 0;

tty tty_table[MAX_TTY];

static device_lock cons;

static void consputc(int);

static inline void update_pos(int pos) {
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
  crt[pos] = ' ' | 0x0700;
}

static void printint(int xx, int base, int sign) {
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign) buf[i++] = '-';

  while (--i >= 0) consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...) {
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if (locking) acquire(&cons.lock);

  if (fmt == 0) panic("null fmt");

  argp = (uint *)(void *)(&fmt + 1);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c != '%') {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0) break;
    switch (c) {
      case 'd':
        printint(*argp++, 10, 1);
        break;
      case 'x':
      case 'p':
        printint(*argp++, 16, 0);
        break;
      case 's':
        if ((s = (char *)*argp++) == 0) s = "(null)";
        for (; *s; s++) consputc(*s);
        break;
      case 'c': {
        s = ((char *)*argp++);
        consputc(*s);
      } break;
      case '%':
        consputc('%');
        break;
      default:
        // Print unknown % sequence to draw attention.
        consputc('%');
        consputc(c);
        break;
    }
  }

  if (locking) release(&cons.lock);
}

void panic(char *s) {
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for (i = 0; i < 10; i++) cprintf(" %p", pcs[i]);
  panicked = 1;  // freeze other CPU
  for (;;) {
  }
}

static void cgaputc(int c) {
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  if (c == '\n')
    pos += 80 - pos % 80;
  else if (c == BACKSPACE) {
    if (pos > 0) --pos;
  } else
    crt[pos++] = (c & 0xff) | 0x0700;  // black on white

  if (pos < 0 || pos > 25 * 80) panic("pos under/overflow");

  if ((pos / 80) >= 24) {  // Scroll up.
    memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
    pos -= 80;
    memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
  }

  update_pos(pos);
}

void consoleclear(void) {
  int pos = 0;
  memset(crt, 0, sizeof(crt[0]) * (24 * 80));
  update_pos(pos);
}

void consputc(int c) {
  if (panicked) {
    cli();
    for (;;) {
    }
  }

  if (c == BACKSPACE) {
    uartputc('\b');
    uartputc(' ');
    uartputc('\b');
  } else
    uartputc(c);
  cgaputc(c);
}

#define INPUT_BUF 128
struct {
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

#define C(x) ((x) - '@')  // Control-x

// ----------- History -------------
static int esc = 0;  // escape sequence flag
#define INPUT_BUF 128
#define ENTRIES 10
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define NOT_VIWING -1

typedef struct {
  char buf[INPUT_BUF];
} history_entry;

typedef struct {
  history_entry history[ENTRIES];
  int index;               // Current command index in history array for saving
  int current_view_index;  // Current command index being viewed
} history;

history cmd_history = {.index = 0, .current_view_index = NOT_VIWING};

// Forward declarations
void print_all_history();
void print_with_consputc(const char *str);

void increment_history_index() {
  // printint(cmd_history.index,10,1);
  cmd_history.index = (cmd_history.index + 1) % ENTRIES;
  // //debug
  // consputc('\n');
  // print_all_history();
  // consputc('\n');
  // consputc('\n');
}

void decrement_history_index() {
  cmd_history.index =
      (cmd_history.index == 0) ? ENTRIES - 1 : cmd_history.index - 1;
}

void save_cmd(char *cmd) {
  //debug
  // print_with_consputc("save_cmd: ");
  // print_with_consputc(cmd);
  // consputc('\n');

  if (!cmd || strlen(cmd) == 0) return;

  // Reset view index when a new command is saved
  cmd_history.current_view_index = -1;

  // Check for duplicate in the entire history
  for (int i = 0; i < ENTRIES; i++) {
    if (strcmp(cmd, cmd_history.history[i].buf) == 0)
      return;  // Duplicate found, do not save
  }

  // check for buffer overflow in history entries - should never happen
  if (cmd_history.index > ENTRIES) {
    panic("History buffer overflow");
  }

// check for buffer overflow in the current entry
// if the command is too long, we will not save it
#ifdef DEBUG
  cprintf("cmd: %s\n", cmd);
#endif
  if (strlen(cmd) > INPUT_BUF) {
    return;
  }

  memset(cmd_history.history[cmd_history.index].buf, 0, INPUT_BUF);

  // Save the command
  strncpy(cmd_history.history[cmd_history.index].buf, cmd, strlen(cmd) + 1);
  cmd_history.history[cmd_history.index].buf[strlen(cmd)] = '\0';
  increment_history_index();
  kfree(cmd);
}

void print_with_consputc(const char *str) {
  // check for valid input for printing
  if (!str) return;

  if (strlen(str) > INPUT_BUF) {
    return;
  }

  if (strlen(str) > SCREEN_WIDTH) {
    // print the first SCREEN_WIDTH characters
    for (int i = 0; i < SCREEN_WIDTH; i++) {
      consputc(str[i]);
    }
    return;
  }

  while (*str) {
    consputc(*str++);
  }
}

void update_input(char *cmd) {
  int len = strlen(cmd);
  if (len >= INPUT_BUF) len = INPUT_BUF - 1;

  memset(input.buf, 0, INPUT_BUF);  // Clear the input buffer
  memcpy(input.buf, cmd, len);      // Copy the command to the input buffer

  // Set the pointers correctly
  input.r = 0;    // Read index at start
  input.w = 0;    // Write index at the end of the command
  input.e = len;  // Edit index at the end of the command
}

void cons_clear_line() {
  while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
    input.e--;
    consputc(BACKSPACE);
  }
}

void print_line_from_history(int line) {
  // delete current command
  cons_clear_line();

  if (line >= ENTRIES) {
    return;
  }
  char *cmd = cmd_history.history[line].buf;
  if (cmd == NULL) {
    return;
  }
  print_with_consputc(cmd);
}

void print_last_command() {
  if (cmd_history.current_view_index == -1) {
    // Start from the most recent command
    cmd_history.current_view_index = cmd_history.index;
  }

  do {
    cmd_history.current_view_index = (cmd_history.current_view_index == 0)
                                         ? ENTRIES - 1
                                         : cmd_history.current_view_index - 1;
    if (*cmd_history.history[cmd_history.current_view_index].buf != '\0') {
      print_line_from_history(cmd_history.current_view_index);
      update_input(cmd_history.history[cmd_history.current_view_index].buf);
      return;
    }
  } while (cmd_history.current_view_index != cmd_history.index);

  // Reset view index if we've reached the current index
  cmd_history.current_view_index = NOT_VIWING;
}

// Function to print the next command in the history
void print_next_command() {
  if (cmd_history.current_view_index == NOT_VIWING) {
    return;  // No history to show
  }

  // delete current command from screen
  cons_clear_line();

  do {
    cmd_history.current_view_index =
        (cmd_history.current_view_index + 1) % ENTRIES;
    if (*cmd_history.history[cmd_history.current_view_index].buf != '\0') {
      print_line_from_history(cmd_history.current_view_index);
      update_input(cmd_history.history[cmd_history.current_view_index].buf);
      return;
    }
  } while (cmd_history.current_view_index != cmd_history.index);

  // Reset view index if we've reached the current index
  cmd_history.current_view_index = NOT_VIWING;
}

// Function to print all commands in the history
void print_all_history() {
  for (int i = 0; i < ENTRIES; i++) {
    consputc('\n');
    printint(i, 10, 0);
    consputc('.');
    consputc(' ');
    char *cmd = cmd_history.history[i].buf;
    if (cmd != NULL) {
      for (int j = 0; j < strlen(cmd); j++) {
        consputc(cmd[j]);
        // notice we don't update the input buffer to avoid overwriting the
      }
    }
  }
  consputc('\n');
  consputc('$');
  consputc(' ');
}
char *parse_input_buffer() {
  int start = input.r % INPUT_BUF;
  int end = (input.e - 1) % INPUT_BUF;  // -1 to exclude the newline character

  char *cmd = (char *)kalloc();
  memset(cmd, 0, INPUT_BUF);

  if (start <= end) {
    // Command does not wrap around
    memcpy(cmd, input.buf + start, end - start);
  } else {
    // Command wraps around
    int firstPartLength = INPUT_BUF - start;
    memcpy(cmd, input.buf + start, firstPartLength);
    memcpy(cmd + firstPartLength, input.buf, end);
  }

  // Manually place a null terminator at the end of the command
  cmd[(end >= start) ? (end - start) : (INPUT_BUF - start + end)] = '\0';

  return cmd;
}

// Return type changed to int to indicate whether to continue or not
int handleEscapeSequence(int c) {
  if (esc == 1 && c == '[') {
    esc = 2;
    return 1;  // Continue the loop in consoleintr
  }

  if (esc == 2) {
    esc = 0;
    switch (c) {
      case 'A':
        print_last_command();
        break;
      case 'B':
        print_next_command();
        break;
      case 'D':
        print_all_history();
        break;
        // TODO: add more cases.
    }
    return 1;  // Continue the loop in consoleintr
  }

  if (c == '\033') {
    esc = 1;
    return 1;  // Continue the loop in consoleintr
  } else {
    esc = 0;
  }

  return 0;  // Do not continue, proceed with the rest of consoleintr logic
}

// -------------------------------

void consoleintr(int (*getc)(void)) {
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while ((c = getc()) >= 0) {
    if (handleEscapeSequence(c)) {
      continue;
    }
    switch (c) {
      case C('P'):  // Process listing.
        // procdump() locks cons.lock indirectly; invoke later
        doprocdump = 1;
        break;
      case C('U'):  // Kill line.
        while (input.e != input.w &&
               input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
          input.e--;
          consputc(BACKSPACE);
        }
        break;
      case C('H'):
      case '\x7f':  // Backspace
        if (input.e != input.w) {
          input.e--;
          consputc(BACKSPACE);
        }
        break;
      default:
        if (c != 0 && input.e - input.r < INPUT_BUF) {
          c = (c == '\r') ? '\n' : c;
          input.buf[input.e++ % INPUT_BUF] = c;
          consputc(c);
          if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF) {
            input.w = input.e;
            if (c == '\n') {
              save_cmd(parse_input_buffer());
            }
            wakeup(&input.r);
          }
        }
        break;
    }
  }
  release(&cons.lock);
  if (doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int ttystat(int minor, struct dev_stat *device_stats) {
  if ((void *)0 == device_stats)
    panic("Invalid device statisitcs structre (NULL)");

  device_stats->rbytes = tty_table[minor].tty_bytes_read;
  device_stats->wbytes = tty_table[minor].tty_bytes_written;
  device_stats->rios = tty_table[minor].ttyread_operations_counter;
  device_stats->wios = tty_table[minor].ttywrite_operations_counter;

  return 0;
}

int consoleread(struct vfs_inode *ip, int n, vector *dstvector) {
  uint target;
  int c;
  int dstindx = 0;
  ip->i_op.iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while (n > 0) {
    while (input.r == input.w) {
      if (myproc()->killed) {
        release(&cons.lock);
        ip->i_op.ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if (c == C('D')) {  // EOF
      if (n < target) {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    memmove_into_vector_bytes(*dstvector, dstindx++, (char *)&c, 1);
    // test for vector \ buffer similarity using silent version of vectormemcmp
    --n;

    /* increment number of bytes read on the specific tty */
    /* TODO: make sure this operation shouldn't be atomic */
    tty_table[ip->minor].tty_bytes_read++;

    if (c == '\n') break;
  }
  release(&cons.lock);
  ip->i_op.ilock(ip);

  return target - n;
}

int ttyread(struct vfs_inode *ip, int n, vector *dstvector) {
  if (tty_table[ip->minor].flags & DEV_CONNECT) {
    /* increment the number of read operations done (even if they
    not succeeded)
    */
    tty_table[ip->minor].ttyread_operations_counter++;
    return consoleread(ip, n, dstvector);
  }

  if (tty_table[ip->minor].flags & DEV_ATTACH) {
    ip->i_op.iunlock(ip);
    acquire(&tty_table[ip->minor].lock);
    sleep(&tty_table[ip->minor], &tty_table[ip->minor].lock);
    // after wakeup has been called
    release(&tty_table[ip->minor].lock);
    ip->i_op.ilock(ip);
  }
  return -1;
}

int consolewrite(struct vfs_inode *ip, char *buf, int n) {
  int i;

  ip->i_op.iunlock(ip);
  acquire(&cons.lock);
  for (i = 0; i < n; i++) {
    consputc(buf[i] & 0xff);
    /* increment the number of bytes written to a specific tty*/
    tty_table[ip->minor].tty_bytes_written++;
  }
  release(&cons.lock);
  ip->i_op.ilock(ip);

  return n;
}

int ttywrite(struct vfs_inode *ip, char *buf, int n) {
  if (tty_table[ip->minor].flags & DEV_CONNECT) {
    /* increment the number of write operations executed on the
    specific tty device (even if its not succeeded) */
    tty_table[ip->minor].ttywrite_operations_counter++;
    return consolewrite(ip, buf, n);
  }
  // 2DO: should return -1 when write to tty fails - filewrite panics.
  return n;
}

void consoleinit(void) {
  initlock(&cons.lock, "console");

  /* init the device driver callback functions */
  devsw[CONSOLE_MAJOR].write = ttywrite;
  devsw[CONSOLE_MAJOR].read = ttyread;
  devsw[CONSOLE_MAJOR].stat = ttystat;
  tty_table[CONSOLE_MINOR].flags = DEV_CONNECT;

  // To state that the console tty is also attached
  // this will make the console sleep whilre we are connected to another tty.
  tty_table[CONSOLE_MINOR].flags |= DEV_ATTACH;
  initlock(&tty_table[CONSOLE_MINOR].lock, "ttyconsole");

  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}

void ttyinit(void) {
  // we create tty devices after the console
  // therefor the tty's minor will be after the console's
  for (int i = CONSOLE_MINOR + 1; i < MAX_TTY; i++) {
    tty_table[i].flags = 0;
    tty_table[i].tty_bytes_read = 0;
    tty_table[i].tty_bytes_written = 0;
    tty_table[i].ttyread_operations_counter = 0;
    tty_table[i].ttywrite_operations_counter = 0;
  }
}

void tty_disconnect(struct vfs_inode *ip) {
  tty_table[ip->minor].flags &= ~(DEV_CONNECT);
  tty_table[CONSOLE_MINOR].flags |= DEV_CONNECT;

  // wakeup the console (it is sleeping now while being attached)
  wakeup(&tty_table[CONSOLE_MINOR]);

  consoleclear();
  cprintf("Console connected\n");
}

void tty_connect(struct vfs_inode *ip) {
  tty_table[ip->minor].flags |= DEV_CONNECT;
  for (int i = CONSOLE_MINOR; i < MAX_TTY; i++) {
    if (ip->minor != i) {
      tty_table[i].flags &= ~(DEV_CONNECT);
    }
  }
  consoleclear();
  cprintf("\ntty%d connected\n", ip->minor - (CONSOLE_MINOR + 1));

  // Wakeup the processes that slept on ttyread()
  wakeup(&tty_table[ip->minor]);
}

void tty_attach(struct vfs_inode *ip) {
  tty_table[ip->minor].flags |= DEV_ATTACH;
  initlock(&(tty_table[ip->minor].lock), "tty");

  /* add the tty device to the current cgroup devices list */
  cgroup_add_io_device(proc_get_cgroup(), ip);
}

void tty_detach(struct vfs_inode *ip) {
  tty_table[ip->minor].flags &= ~(DEV_ATTACH);
  /* Note: We don't clear the tty's stats because the tty exist it is just
  detached from the cgroup. Any reuse of the tty (new cgroup created) will use
  this tty
  */

  /* remove the tty device from the current cgroup devices list */
  cgroup_remove_io_device(proc_get_cgroup(), ip);
}

int tty_gets(struct vfs_inode *ip, int command) {
  return (tty_table[ip->minor].flags & command ? 1 : 0);
}
