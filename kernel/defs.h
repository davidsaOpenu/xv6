#ifndef XV6_DEFS_H
#define XV6_DEFS_H

#include "types.h"

struct buf;
union buf_id;
struct context;
struct file;
struct inode;
struct mount;
struct mount_list;
struct mount_ns;
struct nsproxy;
struct pipe;
struct proc;
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct cgroup;
struct objsuperblock;
struct vfs_inode;
struct vfs_file;
typedef struct kvec vector;
struct devsw;
struct dev_stat;
struct cgroup_io_device_statistics_s;

// bio.c
struct buf* bread(uint, uint);
void bwrite(struct buf*);

// buf_cache.c
void buf_cache_init(void);
void buf_cache_invalidate_blocks(uint dev);
struct buf* buf_cache_get(uint dev, const union buf_id* id, uint alloc_flags);
void buf_cache_release(struct buf* b);
uint buf_cache_is_cache_enabled(void);
void buf_cache_enable_cache(void);
void buf_cache_disable_cache(void);

// console.c
void consoleclear(void);
void consoleinit(void);
void ttyinit(void);
void cprintf(char*, ...);
void consoleintr(int (*)(void));
void panic(char*) __attribute__((noreturn));
void tty_disconnect(struct vfs_inode* ip);
void tty_connect(struct vfs_inode* ip);
void tty_attach(struct vfs_inode* ip);
void tty_detach(struct vfs_inode* ip);
int tty_gets(struct vfs_inode* ip, int command);

// device.c
int getorcreatedevice(struct vfs_inode*);
int createobjdevice();
struct obj_device* objdeviceget(uint dev);
void deviceput(uint);
void deviceget(uint);
struct vfs_inode* getinodefordevice(uint);
void objdevinit(uint dev);
struct vfs_superblock* getsuperblock(uint);
void devinit(void);
int doesbackdevice(struct vfs_inode*);

// exec.c
int exec(char*, char**);

// vfs_file.c
void vfs_fileinit(void);
struct vfs_file* vfs_filealloc(void);
void vfs_fileclose(struct vfs_file*);
struct vfs_file* vfs_filedup(struct vfs_file*);
int vfs_fileread(struct vfs_file*, int n, vector* dstvector);
int vfs_filestat(struct vfs_file*, struct stat*);
int vfs_filewrite(struct vfs_file*, char*, int n);

// obj_fs.c
void obj_fs_init(void);
void obj_fs_init_dev(uint dev);
struct vfs_inode* obj_ialloc(uint, short);
void obj_iinit(void);
struct vfs_inode* obj_iget(uint dev, uint inum);
struct vfs_inode* obj_initprocessroot(struct mount**);

// fs.c
void readsb(int dev, struct superblock* sb);
struct vfs_inode* ialloc(uint, short);
void iinit(uint dev);
struct vfs_inode* iget(uint dev, uint inum);
void fsinit(uint);
struct vfs_inode* initprocessroot(struct mount**);

// vfs_fs.c
struct vfs_inode* vfs_namei(char*);
struct vfs_inode* vfs_nameimount(char*, struct mount**);
struct vfs_inode* vfs_nameiparent(char*, char*);
struct vfs_inode* vfs_nameiparentmount(char*, char*, struct mount**);
int vfs_namecmp(const char*, const char*);
int vfs_namencmp(const char* s, const char* t, int length);

// sysmount.c
int handle_objfs_mounts();
int handle_cgroup_mounts();
int handle_proc_mounts();
int handle_bind_mounts();
int handle_nativefs_mounts();

// mount.c
void mntinit(void);
int mount(struct vfs_inode*, struct vfs_inode*, struct vfs_inode*,
          struct mount*);
int umount(struct mount*);
struct mount* getrootmount(void);
struct mount* mntdup(struct mount*);
void mntput(struct mount*);
struct mount* mntlookup(struct vfs_inode*, struct mount*);
void umountall(struct mount_list*);
struct mount_list* copyactivemounts(void);
struct mount* getroot(struct mount_list*);
struct mount* getinitialrootmount(void);

// ide.c
void ideinit(void);
void ideintr(void);
void iderw(struct buf*);

// ioapic.c
void ioapicenable(int irq, int cpu);
extern uchar ioapicid;
void ioapicinit(void);

// kalloc.c
char* kalloc(void);
void kfree(char*);
void kinit1(void*, void*);
void kinit2(void*, void*);
int kmemtest(void);
int increse_protect_counter(int num);
int decrese_protect_counter(int num);
uint get_total_memory();

// kvector.c
vector newvector(unsigned int, unsigned int);
void freevector(vector*);
uint setelement(vector, unsigned int, char*);
char* getelementpointer(const vector, unsigned int);
void memmove_into_vector_bytes(vector, unsigned int, char*, unsigned int);
void memmove_into_vector_elements(vector, unsigned int, char*, unsigned int);
void memmove_from_vector(char* dst, vector vec, unsigned int elementoffset,
                         unsigned int elementcount);
vector slicevector(vector, unsigned int, unsigned int);
uint vectormemcmp(const vector v, void* m, uint bytes);
uint copysubvector(vector* dstvector, vector* srcvector, unsigned int srcoffset,
                   unsigned int count);

// kbd.c
void kbdintr(void);

// lapic.c
void cmostime(struct rtcdate* r);
int lapicid(void);
extern volatile uint* lapic;
void lapiceoi(void);
void lapicinit(void);
void lapicstartap(uchar, uint);
void microdelay(int);

// log.c
void initlog(int dev);
void log_write(struct buf*);
void begin_op();
void end_op();

// mount_ns.c
void mount_nsinit(void);
void mount_nsput(struct mount_ns*);
struct mount_ns* mount_nsdup(struct mount_ns*);
struct mount_ns* newmount_ns(void);
struct mount_ns* copymount_ns(void);

// mp.c
extern int ismp;
void mpinit(void);

// namespace.c
void namespaceinit(void);
struct nsproxy* emptynsproxy(void);
struct nsproxy* namespacedup(struct nsproxy*);
void namespaceput(struct nsproxy*);
int unshare(int nstype);

// picirq.c
void picenable(int);
void picinit(void);

// pipe.c
int pipealloc(struct vfs_file**, struct vfs_file**);
void pipeclose(struct pipe*, int);
int piperead(struct pipe*, int, vector* outputvector);
int pipewrite(struct pipe*, char*, int);

// PAGEBREAK: 16
//  proc.c
int cpuid(void);
void exit(int);
int fork(void);
int growproc(int);
int kill(int);
struct cpu* mycpu(void);
struct proc* myproc();
void pinit(void);
void procdump(void);
void scheduler(void) __attribute__((noreturn));
void sched(void);
void setproc(struct proc*);
void sleep(void*, struct spinlock*);
void userinit(void);
int wait(int*);
void wakeup(void*);
void yield(void);
int cgroup_move_proc(struct cgroup* cgroup, int pid);

// swtch.S
void swtch(struct context**, struct context*);

// spinlock.c
void acquire(struct spinlock*);
void getcallerpcs(void*, uint*);
int holding(struct spinlock*);
void initlock(struct spinlock*, char*);
void release(struct spinlock*);
void pushcli(void);
void popcli(void);

// sleeplock.c
void acquiresleep(struct sleeplock*);
void releasesleep(struct sleeplock*);
int holdingsleep(struct sleeplock*);
void initsleeplock(struct sleeplock*, char*);

// When running host tests we use the host's libc which
// presents a bit different string method signatures.
#ifdef HOST_TESTS
#include <string.h>  // // NOLINT(build/include_what_you_use)
#else
// string.c
int memcmp(const void*, const void*, uint);
void* memmove(void*, const void*, uint);
void* memset(void*, int, uint);
char* safestrcpy(char*, const char*, int);
int strlen(const char*);
int strncmp(const char*, const char*, uint);
int strcmp(const char* p, const char* q);
char* strncpy(char*, const char*, int);
void* memcpy(void*, const void*, uint);
#endif

// syscall.c
int argint(int, int*);
int argptr(int, char**, int);
int argstr(int, char**);
int fetchint(uint, int*);
int fetchstr(uint, char**);
void syscall(void);
int getppid(void);

// timer.c
void timerinit(void);

// trap.c
void idtinit(void);
extern uint ticks;
void tvinit(void);
extern struct spinlock tickslock;

// uart.c
void uartinit(void);
void uartintr(void);
void uartputc(int);

// vm.c
void seginit(void);
void kvmalloc(void);
pde_t* setupkvm(void);
char* uva2ka(pde_t*, char*);
int allocuvm(pde_t*, uint, uint, struct cgroup* cgroup);
int deallocuvm(pde_t*, uint, uint);
void freevm(pde_t*);
void inituvm(pde_t*, char*, uint);
int loaduvm(pde_t*, char*, struct vfs_inode*, uint, uint);
pde_t* copyuvm(pde_t*, uint);
void switchuvm(struct proc*);
void switchkvm(void);
int copyout(pde_t*, uint, void*, uint);
void clearpteu(pde_t* pgdir, char* uva);
void inc_protect_mem(struct cgroup* cgroup, int n);
int dec_protect_mem(struct cgroup* cgroup);

// cgroup.c
void cginit(void);
void cgroup_add_io_device(struct cgroup* cgroup_ptr, struct vfs_inode* io_node);
void cgroup_remove_io_device(struct cgroup* cgroup_ptr,
                             struct vfs_inode* io_node);

// klib.c
int atoi(const char* str);
int itoa(char* buf, int n);
int utoa(char* buf, unsigned int n);
int intlen(int n);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

// min between two numbers
#define min(x, y) (x) > (y) ? (y) : (x)

// max of two numbers
#define max(x, y) (x) > (y) ? (x) : (y)

// if x is bigger that 0 return x, else return 0
#define at_least_zero(x) (x) > 0 ? (x) : (0)

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/** Return codes:
 * - RESULT_ERROR_OPERATION upon error related to the executed operation.
 * - RESULT_ERROR_ARGUMENT upon error related to argument of the operation.
 * - RESULT_ERROR upon general failure (error).
 * - RESULT_SUCCESS upon general success - no errors.
 * - RESULT_SUCCESS_OPERATION upon successful (effective) operation.
 *
 * E.g.
 * Delete a file operation:
 * 		RESULT_ERROR_OPERATION - can not delete file name, missing
 * permissions RESULT_ERROR_ARGUMENT - file name is not a valid name.
 * 		RESULT_ERROR - general error occurred, deletion service not
 * initialized RESULT_SUCCESS - file not exists, no action taken - no error.
 * 		RESULT_SUCCESS_OPERATION - file exists and deleted without
 * error.
 */
typedef enum {
  RESULT_ERROR_OPERATION = -3,
  RESULT_ERROR_ARGUMENT,
  RESULT_ERROR,
  RESULT_SUCCESS,
  RESULT_SUCCESS_OPERATION
} result_code;

#endif /* XV6_DEFS_H */
