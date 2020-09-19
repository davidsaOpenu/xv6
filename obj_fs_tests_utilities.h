#pragma once

/**
 * Implement in naive way functions needed by the tests.
 * Most implementations does nothing to keep the tests flow.
 */

#include "sleeplock.h"
#include "proc.h"


typedef void(*panic_handler_t)(void);

/// Prints the msg and calls the handler. The default handler calls `exit`.
void panic(const char* msg);
void set_panic_handler(panic_handler_t new_handler);
void default_panic_handler(void);

///@{
///does nothing
void wakeup(const struct sleeplock* lk);
void sleep(void* chan, struct spinlock* lk);
///@}

// tests has their own "proc" which can be manipulated manualy.
struct proc* myproc();
