#ifndef XV6_SYSCALL_H
#define XV6_SYSCALL_H

// System call numbers
#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_pipe 4
#define SYS_read 5
#define SYS_kill 6
#define SYS_exec 7
#define SYS_fstat 8
#define SYS_chdir 9
#define SYS_dup 10
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_sleep 13
#define SYS_uptime 14
#define SYS_open 15
#define SYS_write 16
#define SYS_mknod 17
#define SYS_unlink 18
#define SYS_link 19
#define SYS_mkdir 20
#define SYS_close 21
#define SYS_mount 22
#define SYS_umount 23
#define SYS_unshare 24
#define SYS_usleep 25
#define SYS_ioctl 26
#define SYS_getppid 27
#define SYS_getcpu 28
#define SYS_pivot_root 29

#endif /* XV6_SYSCALL_H */
