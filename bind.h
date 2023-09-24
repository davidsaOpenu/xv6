#ifndef XV6_BIND_H
#define XV6_BIND_H

#include "mount_ns.h"
#include "defs.h"
#include "mmu.h"
#include "mount.h"
#include "mount_ns.h"
#include "namespace.h"
#include "param.h"
#include "spinlock.h"
#include "types.h"
#include "proc.h"

void bindtableinit(void);

struct bind_mount_list* get_bindtable(void);

char* apply_binds(char* path);

void add_root_dir_to_bind_table(struct bind_mount_list* bindmnt, char * root_dir);

#endif /* XV6_BIND_H */