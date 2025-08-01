#ifndef XV6_USER_H
#define XV6_USER_H

#include "ioctl_request.h"
#include "types.h"

struct stat;
struct rtcdate;

#define stdin (0)
#define stdout (1)
#define stderr (2)

#define IMAGE_DIR "/images/"
#define DEV_DIR "/dev/"

// system calls
int fork(void);
int exit(int status) __attribute__((noreturn));
int wait(int* wstatus);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, const char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkdir(const char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int usleep(unsigned int);
int uptime(void);
int ioctl(int fd, unsigned long request, ...);
int getppid(void);
int getcpu(void);

int mount(const char*, const char*, const char*);
int umount(const char*);
int unshare(int);
int pivot_root(const char*, const char*);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void* memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, int);
char* strstr(char* src, char* needle);
char* strtok_r(char* str, char* const delim, char** saveptr);

int printf(int, const char*, ...);
void perror(const char*);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int itoa(char*, int);
char* strcat(char* dest, const char* source);
char* strdup(const char* s);

bool isspace(char c);
bool isalnum(char c);

int attach_tty(int tty_fd);
int detach_tty(int tty_fd);
int connect_tty(int tty_fd);
int is_attached_tty(int tty_fd);
int disconnect_tty(int tty_fd);
int is_connected_tty(int tty_fd);

bool fmtname(const char* const path, char* const out_name, const int out_size);

int system(const char* command);

#define ASSERT(x)                                                 \
  if (!(x)) {                                                     \
    printf(stderr, "%s:%d: assert failed\n", __FILE__, __LINE__); \
    exit(0);                                                      \
  }

#endif /* XV6_USER_H */
