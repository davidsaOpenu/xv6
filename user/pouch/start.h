#ifndef XV6_USER_POUCH_START_H
#define XV6_USER_POUCH_START_H

#include "pouch.h"

/**
 * Entrypoint for the pouch start command.
 * Starts a container with the given name and image.
 */
pouch_status pouch_do_container_start(const char* container_name,
                                      const char* const image_name);

/*
 *   Pouch stop:
 *   - Stopping container
 *   @input: container_name - container name to stop
 */
pouch_status pouch_do_container_stop(const char* const container_name);

#endif  // XV6_USER_POUCH_START_H
