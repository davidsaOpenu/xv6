#ifndef XV6_BIND_H
#define XV6_BIND_H

/* Initialized the the global bind table */
void bindtableinit(void);
/* Finds and returns an empty entry in the global bind table */
struct bind_mount_list* get_bindtable(void);

/* Called when the file system attempts to open a path,
 * if the path is a source in the bind_mount_list, applies the bind
 * This function supports applying only one bind mount.
 * It will replace the beginning of the path with the target
 * of the found bind mount.
 */
char* apply_binds(char* path);

/* Gets a dir path and a bind_mount_table
 * adds a bind from '/' to the given path
 * to the input table
 * effectively creating a bind from a given dir to root dir
 */
void add_root_dir_to_bind_table(struct bind_mount_list* bindmnt,
                                char* root_dir);

#endif /* XV6_BIND_H */
