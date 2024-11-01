#include "fcntl.h"
#include "stat.h"
#include "types.h"
#include "user.h"
#include "wstatus.h"
#include "x86.h"

char *strcpy(char *s, const char *t) {
  char *os;

  os = s;
  while ((*s++ = *t++) != 0) {
  }
  return os;
}

int strcmp(const char *p, const char *q) {
  while (*p && *p == *q) p++, q++;
  return (uchar)*p - (uchar)*q;
}

int strncmp(const char *p, const char *q, int n) {
  while (*p && *p == *q && --n) p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint strlen(const char *s) {
  int n;

  for (n = 0; s[n]; n++) {
  }
  return n;
}

void *memset(void *dst, int c, uint n) {
  stosb(dst, c, n);
  return dst;
}

char *strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c) return (char *)s;
  return 0;
}

char *gets(char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(0, &c, 1);
    if (cc < 1) continue;
    buf[i++] = c;
    if (c == '\n' || c == '\r') break;
  }
  buf[i] = '\0';
  return buf;
}

int stat(const char *n, struct stat *st) {
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if (fd < 0) return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int atoi(const char *s) {
  int n;

  n = 0;
  while ('0' <= *s && *s <= '9') n = n * 10 + *s++ - '0';
  return n;
}

void *memmove(void *vdst, const void *vsrc, int n) {
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  while (n-- > 0) *dst++ = *src++;
  return vdst;
}

/*
 * Set buf to string representation of number in int.
 */
int itoa(char *buf, int n) {
  int i = n;
  int length = 0;

  while (i > 0) {
    length++;
    i /= 10;
  }

  if (n == 0) {
    buf[0] = '0';
    length++;
  }
  for (i = length; n > 0 && i > 0; i--) {
    buf[i - 1] = (n % 10) + '0';
    n /= 10;
  }
  buf[length] = '\0';
  return length;
}

char *strcat(char *dest, const char *source) {
  int i, j;

  for (i = 0; dest[i] != '\0'; i++) {
  }
  for (j = 0; source[j] != '\0'; j++) {
    dest[i + j] = source[j];
  }

  dest[i + j] = '\0';

  return dest;
}

char *strstr(char *src, char *needle) {
  uint i = 0;
  uint needle_size = strlen(needle);
  uint src_len = strlen(src);

  for (i = 0; i < src_len; i++) {
    if (0 == strncmp(src, needle, needle_size)) {
      return src;
    }
    src++;
  }
  return 0;
}

char *strtok_r(char *str, char *const delim, char **saveptr) {
  // use saveptr if not a new string.
  if (str == NULL) {
    if (saveptr == NULL) return NULL;
    str = *saveptr;
  }

  // No more tokens.
  if (*str == '\0') {
    if (saveptr != NULL) *saveptr = str;
    return NULL;
  }

  for (; *str; ++str) {
    bool any_del_found = false;
    for (char *currd = delim; *currd; ++currd) {
      if (*str == *currd) {
        any_del_found = true;
        break;
      }
    }

    if (!any_del_found) {
      break;
    }
  }
  char *to_return = str;
  for (;; ++str) {
    for (char *currd = delim; *currd; ++currd) {
      if (*str == *currd || *str == '\0') {
        *str = '\0';
        if (saveptr != NULL) *saveptr = str + 1;
        return to_return;
      }
    }
  }
  return to_return;
}

bool isspace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

bool isalnum(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

bool fmtname(const char *const path, char *const out_name, const int out_size) {
  const char *pstart, *pend;

  // Find first character after last slash.
  for (pstart = path + strlen(path); pstart >= path && *pstart != '/';
       --pstart) {
  };
  pstart++;
  // stop on first null/space after pstart
  for (pend = pstart; *pend && !isspace(*pend); ++pend) {
  };

  if (pend - pstart > out_size - 1) {
    return false;
  }

  memmove(out_name, pstart, pend - pstart);
  memset(out_name + (pend - pstart), ' ', out_size - (pend - pstart));
  out_name[out_size - 1] = '\0';

  return true;
}

char *strdup(const char *s) {
  char *new_s = (char *)malloc(strlen(s) + 1);
  if (new_s == NULL) {
    return NULL;
  }
  strcpy(new_s, s);
  return new_s;
}

int system(const char *command) {
  // Do the same as rm_recursive, but with the given command.
  int pid = fork();
  if (pid < 0) {
    printf(stderr, "system: fork failed\n");
    return -1;
  }
  if (pid == 0) {
    // sh -c command:
    const char *argv[4] = {0};
    argv[0] = "/sh";
    argv[1] = "-c";
    argv[2] = command;
    exec(argv[0], argv);
    printf(stderr, "system: exec failed\n");
    exit(1);
  } else {
    int wstatus = 0;
    if (wait(&wstatus) < 0) {
      printf(stderr, "system: wait failed\n");
      return -1;
    }
    return WEXITSTATUS(wstatus);
  }
}
