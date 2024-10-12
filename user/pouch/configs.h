/**
 * Pouch configuration file management utilities
 */
#ifndef XV6_USER_POUCH_CONFIGS_H
#define XV6_USER_POUCH_CONFIGS_H
#include "container.h"
#include "lib/user.h"
#include "pouch.h"

#define CONFIG_KEY_TTYNUM "TTYNUM:"
#define CONFIG_KEY_PPID "PPID:"
#define CONFIG_KEY_NAME "NAME:"
#define CONFIG_KEY_IMAGE "IMAGE:"

#define POUCH_CONFIGS_DIR "/pconf/"

#define TTYNAMESIZE sizeof(DEV_DIR "ttyX")

typedef struct container_config {
  char container_name[CNTNAMESIZE];
  int tty_num;
  int pid;
  char image_name[CNTNAMESIZE];
} container_config;

pouch_status pouch_pconf_init();

/*
 *   Write to pconf
 *   - pconf is a file that holds container name that is currently attached to a
 * tty
 *   - pconf name is similar to tty name, having knows naming structure to open
 * it
 *   - used for 'printing all containers list 'pouch list' command
 *   @input: tty_num, cname
 */
pouch_status pouch_pconf_write(const int tty_num, const char* const cname);

/*
 *   Remove from pconf
 *   - when container is deleted, tty is detatched, need to remove it's name
 * from pconf
 *   @input: tty_num
 */
pouch_status pouch_pconf_remove(const int tty_num);
/*
 *   Read from pconf
 *   - get container name from provided tty name
 *   @input: tty_num
 *   @output: cname
 */
pouch_status pouch_pconf_get_ttyname(int tty_num, char* const cname);

/*
 *   Read from conf:
 *   - Reading container information from container's object
 *   @input: container_name
 *   @output: conf (optional)
 */
pouch_status pouch_cconf_read(const char* const container_name,
                              container_config* conf);

/*
 *   Write to conf:
 *   - Writing container information to container's object
 *   @input: conf
 *   @output: none
 */
pouch_status pouch_cconf_write(const container_config* conf);

/*
 * Delete the cconf file for the given container name.
 */
int pouch_cconf_unlink(const container_config* const conf);

/*
 * Open tty fd by tty number (index) and mode.
 */
int pouch_open_tty(const int tty_num, const int mode);

#endif  // XV6_USER_POUCH_CONFIGS_H
