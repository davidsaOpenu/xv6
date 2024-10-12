#include "mount_ns.h"

#include "defs.h"
#include "mmu.h"
#include "mount.h"
#include "namespace.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

struct {
  struct spinlock lock;
  struct mount_ns mount_ns[NNAMESPACE];
} mountnstable;

static struct mount_ns* allocmount_ns() {
  acquire(&mountnstable.lock);
  for (int i = 0; i < NNAMESPACE; i++) {
    if (mountnstable.mount_ns[i].ref == 0) {
      struct mount_ns* mount_ns = &mountnstable.mount_ns[i];
      mount_ns->ref = 1;
      release(&mountnstable.lock);
      return mount_ns;
    }
  }
  release(&mountnstable.lock);

  panic("out of mount_ns objects");
}

void mount_nsinit() {
  initlock(&mountnstable.lock, "mountns");
  for (int i = 0; i < NNAMESPACE; i++) {
    initlock(&mountnstable.mount_ns[i].lock, "mount_ns");
  }

  if (allocmount_ns() != get_root_mount_ns()) {
    panic("mount_nsinit double");
  }
}

struct mount_ns* mount_nsdup(struct mount_ns* mount_ns) {
  acquire(&mountnstable.lock);
  mount_ns->ref++;
  release(&mountnstable.lock);

  return mount_ns;
}

void mount_nsput(struct mount_ns* mount_ns) {
  acquire(&mountnstable.lock);
  if (mount_ns->ref == 1) {
    release(&mountnstable.lock);

    umountall(mount_ns->active_mounts);
    mount_ns->active_mounts = 0;

    acquire(&mountnstable.lock);
  }
  mount_ns->ref--;
  release(&mountnstable.lock);
}

struct mount_ns* copymount_ns() {
  struct mount_ns* mount_ns = allocmount_ns();
  mount_ns->active_mounts = copyactivemounts();
  mount_ns->root = getroot(mount_ns->active_mounts);
  return mount_ns;
}

struct mount_ns* get_root_mount_ns() {
  struct mount_ns* ns = &mountnstable.mount_ns[0];
  return ns;
}

void set_mount_ns_root(struct mount_ns* ns, struct mount* root) {
  acquire(&ns->lock);
  ns->root = root;
  release(&ns->lock);
}
