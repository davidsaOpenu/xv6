/*
 *   Pouch.h
 *   - pouch container commands
 */
#ifndef XV6_POUCH_H
#define XV6_POUCH_H

typedef enum p_cmd {
  START,
  CONNECT,
  DISCONNECT,
  DESTROY,
  LIMIT,
  INFO,
  LIST,
  IMAGES,
  BUILD,
} p_cmd;
#define CNTNAMESIZE 100
#define CNTARGSIZE 30
char* argv[] = {"sh", 0};

/*
 *   Pouch cmd:
 *   - Pouch operation based on command type
 *   @input: container_name,image_name,pouch_file,p_cmd
 *   @output: none
 *   @return: 0 - OK, != 0 - FAILURE
 */
static int pouch_cmd(char* container_name, char* image_name, enum p_cmd);

/*
 *   Pouch fork:
 *   - Starting new container and execute shell inside, waiting for container to
 * exit
 *   @input: container_name,root_dir
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int pouch_fork(char* container_name, char* image_name);

/*
 *   Finding a tty:
 *   - Finds a free tty to be attached
 *   @input: none
 *   @output: tty_name
 *   @return: 0 - OK, <0 - FAILURE
 */
static int find_tty(char* tty_name);

/*
 *   Read from conf:
 *   - Reading container information from container's object
 *   @input: container_name
 *   @output: tty_name, pid
 *   @return: 0 - OK, <0 - FAILURE
 */
static int read_from_cconf(char* container_name, char* tty_name, int* pid,
                           char* image_name);

/*
 *   Write to conf:
 *   - Writing container information to container's object
 *   @input: container_name, tty_name, pid
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int write_to_cconf(char* container_name, char* tty_name, int pid,
                          char* image_name);

/*
 *   Init pouch cgroup:
 *   - Creates root cgroup dir if not exists and mounts cgroups fs
 *   @input: none
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int init_pouch_cgroup();

/*
 *   Create pouch cgroup:
 *   - Creates cgroup for a container inside root cgroup
 *   - Enables cpu.controller
 *   @input: cg_cname - cgroups fs path to a new cgroup
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int create_pouch_cgroup(char* cg_cname, char* cname);

/*
 *   Limit pouch cgroup:
 *   - Limits given state object for given container name and limit
 *   @input: container_name, cgroup_state_obj, limitation
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int pouch_limit_cgroup(char* container_name, char* cgroup_state_obj,
                              char* limitation);

/*
 *   Prepate cgroup name:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: container_name
 *   @output: cgcname
 */
static void prepare_cgroup_cname(const char* container_name, char* cgcname);

/*
 *   Prepare image mount path:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: container_name
 *   @output: image_mount_point
 */
static void prepare_image_mount_path(const char* container_name,
                                     char* image_mount_point);

/*
 *   Prepare image name to path:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: image_name
 *   @output: image_path
 */
static void image_name_to_path(const char* image_name, char* image_path);

/*
 *   Write to pconf
 *   - pconf is a file that holds container name that is currently attached to a
 * tty
 *   - pconf name is similar to tty name, having knows naming structure to open
 * it
 *   - used for 'printing all containers list 'pouch list all' command
 *   @input: container_name, cgcname
 *   @return: 0 - OK, <0 - FAILURE
 */
static int write_to_pconf(char* ttyname, char* cname);

/*
 *   Remove from pconf
 *   - when container is deleted, tty is detatched, need to remove it's name
 * from pconf
 *   @input: ttyname
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int remove_from_pconf(char* ttyname);

/*
 *   Read from pconf
 *   - get container name from provided tty name
 *   @input: ttyname
 *   @output: cname
 *   @return: 0 - OK, <0 - FAILURE
 */
static int read_from_pconf(char* ttyname, char* cname);

/*
 *   Print cotainers list
 *   - show all started containers and their state
 *   @input: none
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int print_clist();

/*
 *   Print given container information
 *   - show all started containers and their state
 *   @input: container_name,tty_name,pid
 *   @output: none
 *   @return: 0 - OK, <0 - FAILURE
 */
static int print_cinfo(char* container_name, char* tty_name, int pid);

/*
 *   Get connected container name
 *   @input: none
 *   @output: cname
 *   @return: 0 - OK, <0 - FAILURE
 */
static int get_connected_cname(char* cname);

/*
 *   Get all avaliable images
 *   @input: none
 *   @output: prints all avaliable images
 *   @return: 0 - OK, <0 - FAILURE
 */
static int pouch_print_images();
static char* fmtname(char* path);

#endif /* XV6_POUCH_H */
