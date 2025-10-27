// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "console.h"

#include "defs.h"
#include "fcntl.h"
#include "fs/vfs_file.h"
#include "fsdefs.h"
#include "kvector.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "pid_ns.h"
#include "proc.h"
#include "sleeplock.h"
#include "traps.h"
#include "types.h"
#include "x86.h"

#define CONSOLE_LINE_LENGTH (80)
#define CONSOLE_VIEWABLE_NUM_OF_LINE (24)

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
    pos += CONSOLE_LINE_LENGTH - pos % CONSOLE_LINE_LENGTH;
  else if (c == BACKSPACE) {
    if (pos > 0) --pos;
  } else
    crt[pos++] = (c & 0xff) | 0x0700;  // black on white

  if (pos < 0 || pos > (CONSOLE_VIEWABLE_NUM_OF_LINE + 1) * CONSOLE_LINE_LENGTH)
    panic("pos under/overflow");

  if ((pos / CONSOLE_LINE_LENGTH) >=
      CONSOLE_VIEWABLE_NUM_OF_LINE) {  // Scroll up.
    memmove(crt, crt + CONSOLE_LINE_LENGTH,
            sizeof(crt[0]) * (CONSOLE_VIEWABLE_NUM_OF_LINE - 1) *
                CONSOLE_LINE_LENGTH);
    pos -= CONSOLE_LINE_LENGTH;
    memset(crt + pos, 0,
           sizeof(crt[0]) *
               (CONSOLE_VIEWABLE_NUM_OF_LINE * CONSOLE_LINE_LENGTH - pos));
  }

  update_pos(pos);
}

void consoleclear(void) {
  int pos = 0;
  memset(crt, 0,
         sizeof(crt[0]) * (CONSOLE_VIEWABLE_NUM_OF_LINE * CONSOLE_LINE_LENGTH));
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

struct {
  char buf[INPUT_BUF_SIZE_BYTES];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} input;

static inline int cmd_history_circular_inc(int idx) {
  return (idx + 1) % CMD_HISTORY_SIZE;
}

static inline int cmd_history_circular_dec(int idx) {
  return (idx - 1 + CMD_HISTORY_SIZE) % CMD_HISTORY_SIZE;
}

static inline int cmd_history_empty(cmd_history *hist) { return !hist->count; }

#define CURSOR_EMPTY_PROMPT -1

// Initializes the command history structure
__attribute__((unused)) static void cmd_history_init(cmd_history *hist) {
  hist->write_idx = 0;
  hist->count = 0;
  hist->cursor = CURSOR_EMPTY_PROMPT;  // Represents a fresh prompt
}

// Appends a command to history
__attribute__((unused)) static void cmd_history_append(cmd_history *hist,
                                                       char *cmd, int size) {
  if (!cmd || !size) return;

  // Copy command to history (limit to INPUT_BUF_SIZE_BYTES - 1 for null
  // terminator)
  int copy_size =
      size < INPUT_BUF_SIZE_BYTES - 1 ? size : INPUT_BUF_SIZE_BYTES - 1;
  for (int i = 0; i < copy_size; i++) hist->data[hist->write_idx][i] = cmd[i];
  hist->data[hist->write_idx][copy_size] = '\0';

  hist->write_idx = cmd_history_circular_inc(hist->write_idx);

  // Increment count by 1 until it reaches the maximum then keep it
  hist->count = hist->count + (1 * (hist->count != CMD_HISTORY_SIZE));

  // Reset cursor upon a write
  hist->cursor = CURSOR_EMPTY_PROMPT;
}

// Moves cursor forward and retrieves the entry (returns null if at a fresh
// prompt)
__attribute__((unused)) static char *cmd_history_next(cmd_history *hist) {
  // Check we are not at a fresh prompt
  if (cmd_history_empty(hist) || hist->cursor == CURSOR_EMPTY_PROMPT)
    return NULL;

  // If at the most recent entry, move to a fresh prompt
  if (hist->cursor == cmd_history_circular_dec(hist->write_idx)) {
    hist->cursor = CURSOR_EMPTY_PROMPT;
    return NULL;
  }

  // Move cursor forward
  hist->cursor = cmd_history_circular_inc(hist->cursor);

  return hist->data[hist->cursor];
}

// Moves cursor backward and retrieves the entry (returns null if at the bottom)
__attribute__((unused)) static char *cmd_history_prev(cmd_history *hist) {
  // 0 if write_idx hasn't wrapped around, write_idx otherwise
  int bottom_idx = hist->write_idx * (hist->count == CMD_HISTORY_SIZE);

  // Check we are not at the history bottom
  if (cmd_history_empty(hist) || hist->cursor == bottom_idx) return NULL;

  if (hist->cursor == CURSOR_EMPTY_PROMPT) {
    // If at a fresh prompt, move to the most recent entry
    hist->cursor = cmd_history_circular_dec(hist->write_idx);
  } else {
    // Move the cursor backward
    hist->cursor = cmd_history_circular_dec(hist->cursor);
  }

  return hist->data[hist->cursor];
}

#define C(x) ((x) - '@')  // Control-x

void consoleintr(int (*getc)(void)) {
  int c, doprocdump = 0;

  acquire(&cons.lock);
  while ((c = getc()) >= 0) {
    switch (c) {
      case C('P'):  // Process listing.
        // procdump() locks cons.lock indirectly; invoke later
        doprocdump = 1;
        break;
      case C('U'):  // Kill line.
        while (input.e != input.w &&
               input.buf[(input.e - 1) % INPUT_BUF_SIZE_BYTES] != '\n') {
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
        if (c != 0 && input.e - input.r < INPUT_BUF_SIZE_BYTES) {
          c = (c == '\r') ? '\n' : c;
          input.buf[input.e++ % INPUT_BUF_SIZE_BYTES] = c;
          consputc(c);
          if (c == '\n' || c == C('D') ||
              input.e == input.r + INPUT_BUF_SIZE_BYTES) {
            input.w = input.e;
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
  ip->i_op->iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while (n > 0) {
    while (input.r == input.w) {
      if (myproc()->killed) {
        release(&cons.lock);
        ip->i_op->ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF_SIZE_BYTES];
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
  ip->i_op->ilock(ip);

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
    ip->i_op->iunlock(ip);
    acquire(&tty_table[ip->minor].lock);
    sleep(&tty_table[ip->minor], &tty_table[ip->minor].lock);
    // after wakeup has been called
    release(&tty_table[ip->minor].lock);
    ip->i_op->ilock(ip);
  }
  return -1;
}

int consolewrite(struct vfs_inode *ip, char *buf, int n) {
  int i;

  ip->i_op->iunlock(ip);
  acquire(&cons.lock);
  for (i = 0; i < n; i++) {
    consputc(buf[i] & 0xff);
    /* increment the number of bytes written to a specific tty*/
    tty_table[ip->minor].tty_bytes_written++;
  }
  release(&cons.lock);
  ip->i_op->ilock(ip);

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
