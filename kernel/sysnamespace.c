#include "fs.h"
#include "param.h"
#include "stat.h"
#include "types.h"
#include "defs.h"
#include "file.h"
#include "mmu.h"
#include "mount.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"

int sys_unshare(void) {
  int nstype;
  if (argint(0, &nstype) < 0) {
    return -1;
  }

  return unshare(nstype);
}
