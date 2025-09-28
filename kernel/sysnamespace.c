#include "defs.h"
#include "mmu.h"
#include "mount.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

int sys_unshare(void) {

  int nstype;
  if (argint(0, &nstype) < 0) {
    return -1;
  }

  return unshare(nstype);
}
