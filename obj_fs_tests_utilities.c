#include <stdio.h>
#include <stdlib.h>

#include "obj_fs_tests_utilities.h"


static panic_handler_t phandler = default_panic_handler;

void panic(const char* msg) {
    printf("kernel panic: %s\n", msg);
    (*phandler)();
}

void set_panic_handler(panic_handler_t new_handler) {
    phandler = new_handler;
}

void default_panic_handler(void) {
    exit(1);
}

void wakeup(const struct sleeplock* lk) {
    //does nothing
}

void sleep(void* chan, struct spinlock* lk) {
    //does nothing
}


static struct proc proc;

struct proc* myproc() {
    return &proc;
}
