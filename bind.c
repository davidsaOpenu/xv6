#include "bind.h"


struct {
  struct spinlock bind_mount_lock;
  struct bind_mount_list bind_table[NBINDTABLE];
} bind_table_holder;

void bindtableinit(void) {
  initlock(&bind_table_holder.bind_mount_lock, "bind_table");
  int i;
  for (i = 0; i < NBINDTABLE; i++) {
    memset(bind_table_holder.bind_table[i].source, '\0', MAX_PATH_LENGTH);
    memset(bind_table_holder.bind_table[i].target, '\0', MAX_PATH_LENGTH);
    bind_table_holder.bind_table[i].next = NULL;
  }
}

static struct bind_mount_list *allocbindtable(void) {
  acquire(&bind_table_holder.bind_mount_lock);
  int i;
  // Find empty mount struct
  for (i = 0; i < NBINDTABLE && bind_table_holder.bind_table[i].source[0] != '\0'; i++) {
  }

  if (i == NBINDTABLE) {
    // error - no available mount memory.
    panic("out of mount_list objects");
  }

  struct bind_mount_list *newbindtable = &bind_table_holder.bind_table[i];
  //newbindtable->next = NULL;

  release(&bind_table_holder.bind_mount_lock);

  return newbindtable;
}

struct bind_mount_list* get_bindtable(void){
  return allocbindtable();
}

void add_root_dir_to_bind_table(struct bind_mount_list* bindmnt, char * root_dir){
    strncpy(bindmnt->source, "/", 2);
    strncpy(bindmnt->target, root_dir, strlen(root_dir));
}

// This function supports applying only one bind mount.
// It will replace the beginning of the path with the target of the found bind mount.
char* apply_binds(char* path){
    //cprintf("path: %s\n", path);
    // static char new_path[MAX_PATH_LENGTH];
    struct bind_mount_list* bind_table = myproc()->nsproxy->mount_ns->bind_table;
    while (bind_table != NULL){
        // cprintf("path: %s. strlen(source): %d, strcmp: %d, source: %s, target: %s\n", path, strlen(bind_table->source), strncmp(path, bind_table->source, strlen(bind_table->source)),
        //  bind_table->source, bind_table->target);
        char * new_path = kalloc();
        if (*path != '/'){
          safestrcpy(new_path, myproc()->cwdp, strlen(myproc()->cwdp) + 1);
          safestrcpy(new_path + strlen(myproc()->cwdp), path, strlen(path) + 1);
          path = new_path;
        }
        if (strncmp(path, bind_table->source, strlen(bind_table->source)) == 0){
            new_path = kalloc();
            safestrcpy(new_path, bind_table->target, strlen(bind_table->target) + 1);
            safestrcpy(new_path + strlen(bind_table->target), path + strlen(bind_table->source), strlen(path + strlen(bind_table->source)) + 1);
            return new_path;
        }
        bind_table = bind_table->next;
    }
    return path;
}