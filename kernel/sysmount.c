//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into vfs_file.c and vfs_fs.c.
//

#include "cgroup.h"
#include "defs.h"
#include "device/ide_device.h"
#include "device/loop_device.h"
#include "device/obj_device.h"
#include "mmu.h"
#include "mount.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

int sys_mount(void) {
  char *fstype;

  if (argstr(2, &fstype) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  // Mount objfs file system
  if (strcmp(fstype, "objfs") == 0) {
    return handle_objfs_mounts();
  } else if (strcmp(fstype, "cgroup") == 0) {
    return handle_cgroup_mounts();
  } else if (strcmp(fstype, "proc") == 0) {
    return handle_proc_mounts();
  } else if (strcmp(fstype, "bind") == 0) {
    return handle_bind_mounts();
  } else {
    return handle_nativefs_mounts();
  }
}

int sys_umount(void) {
  char *mount_path;
  if (argstr(0, &mount_path) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  int delete_cgroup_res = cgroup_delete(mount_path, "umount");

  if (delete_cgroup_res == RESULT_ERROR_ARGUMENT) {
    struct vfs_inode *mount_dir;
    struct mount *mnt;

    if ((mount_dir = vfs_nameimount(mount_path, &mnt)) == 0) {
      end_op();
      return -1;
    }

    mount_dir->i_op->iput(mount_dir);

    int res = umount(mnt);
    if (res != 0) {
      mntput(mnt);
    }

    end_op();
    return res;
  }

  if (delete_cgroup_res == RESULT_ERROR_ARGUMENT) {
    end_op();
    cprintf("cannot unmount cgroup\n");
    return -1;
  }

  end_op();
  return delete_cgroup_res;
}

int handle_objfs_mounts() {
  char *mount_path;
  struct mount *parent;
  struct vfs_inode *mount_dir;
  int res = 0;

  if (argstr(1, &mount_path) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  if ((mount_dir = vfs_nameimount(mount_path, &parent)) == 0) {
    end_op();
    return -1;
  }

  mount_dir->i_op->ilock(mount_dir);

  struct device *objdev = create_obj_device();
  if (objdev == NULL) {
    cprintf("failed to create ObjFS device\n");
    mount_dir->i_op->iunlockput(mount_dir);
    res = -1;
    goto end;
  }

  res = mount(mount_dir, objdev, NULL, parent);
  mount_dir->i_op->iunlock(mount_dir);
  if (res != 0) {
    mount_dir->i_op->iput(mount_dir);
  }
  deviceput(objdev);

end:
  mntput(parent);
  end_op();

  return res;
}

int handle_cgroup_mounts() {
  char *device_path;
  char *mount_path;
  struct mount *parent;
  struct vfs_inode *mount_dir;
  if (argstr(0, &device_path) < 0 || argstr(1, &mount_path) < 0 ||
      device_path != 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  if ((mount_dir = vfs_nameimount(mount_path, &parent)) == 0) {
    cprintf("bad mount_path\n");
    end_op();
    return -1;
  }

  if (*(cgroup_root()->cgroup_dir_path)) {
    cprintf("cgroup filesystem already mounted\n");
    end_op();
    return -1;
  }

  set_cgroup_dir_path(cgroup_root(), mount_path);

  end_op();

  return 0;
}

int handle_proc_mounts() {
  char *device_path;
  char *mount_path;
  struct mount *parent;
  struct vfs_inode *mount_dir;
  if (argstr(0, &device_path) < 0 || argstr(1, &mount_path) < 0 ||
      device_path != 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  if ((mount_dir = vfs_nameimount(mount_path, &parent)) == 0) {
    cprintf("bad mount_path\n");
    end_op();
    return -1;
  }

  if (*procfs_root) {
    cprintf("proc filesystem already mounted\n");
    end_op();
    return -1;
  }

  set_procfs_dir_path(mount_path);

  end_op();

  return 0;
}

int handle_bind_mounts() {
  char *bind_path;
  char *mount_path;
  struct mount *parent;
  struct vfs_inode *mount_dir;
  struct vfs_inode *target_mount_dir;
  if (argstr(0, &bind_path) < 0 || argstr(1, &mount_path) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  if ((target_mount_dir = vfs_namei(bind_path)) == 0) {
    cprintf("bad bind mount path\n");
    end_op();
    return -1;
  }

  if ((mount_dir = vfs_nameimount(mount_path, &parent)) == 0) {
    cprintf("bad mount directory\n");
    target_mount_dir->i_op->iput(target_mount_dir);
    end_op();
    return -1;
  }

  if (mount_dir->inum == ROOTINO) {
    cprintf("Can't mount root directory\n");
    mount_dir->i_op->iput(mount_dir);
    target_mount_dir->i_op->iput(target_mount_dir);
    mntput(parent);
    end_op();
    return -1;
  }

  mount_dir->i_op->ilock(mount_dir);

  if (mount_dir->type != T_DIR) {
    cprintf("mount point is not a directory\n");
    mount_dir->i_op->iunlockput(mount_dir);
    mount_dir->i_op->iput(mount_dir);
    target_mount_dir->i_op->iput(target_mount_dir);
    mntput(parent);
    end_op();
    return -1;
  }

  int res = mount(mount_dir, NULL, target_mount_dir, parent);

  mount_dir->i_op->iunlock(mount_dir);

  if (res != 0) {
    mount_dir->i_op->iput(mount_dir);
    target_mount_dir->i_op->iput(target_mount_dir);
  }

  mntput(parent);

  end_op();

  return 0;
}
int handle_nativefs_mounts() {
  char *device_path;
  char *mount_path;
  struct mount *parent;
  struct vfs_inode *loop_inode, *mount_dir;
  int res = -1;
  if (argstr(0, &device_path) < 0 || argstr(1, &mount_path) < 0) {
    cprintf("badargs\n");
    return -1;
  }

  begin_op();

  if ((loop_inode = vfs_namei(device_path)) == 0) {
    cprintf("bad device_path\n");
    goto exit;
  }

  if ((mount_dir = vfs_nameimount(mount_path, &parent)) == 0) {
    loop_inode->i_op->iput(loop_inode);
    goto exit;
  }

  if (mount_dir->inum == ROOTINO) {
    loop_inode->i_op->iput(loop_inode);
    mount_dir->i_op->iput(mount_dir);
    mntput(parent);
    goto exit;
  }

  loop_inode->i_op->ilock(loop_inode);
  mount_dir->i_op->ilock(mount_dir);

  if (mount_dir->type != T_DIR) {
    loop_inode->i_op->iunlockput(loop_inode);
    mount_dir->i_op->iunlockput(mount_dir);
    mntput(parent);
    goto exit;
  }

  struct device *loop_dev = get_loop_device(loop_inode);
  if (loop_dev == NULL) {
    loop_dev = create_loop_device(loop_inode);
  }

  if (loop_dev == NULL) {
    loop_inode->i_op->iunlockput(loop_inode);
    mount_dir->i_op->iunlockput(mount_dir);
    mntput(parent);
    goto exit;
  }

  res = mount(mount_dir, loop_dev, NULL, parent);

  mount_dir->i_op->iunlock(mount_dir);
  if (res != 0) {
    mount_dir->i_op->iput(mount_dir);
  }

  loop_inode->i_op->iunlockput(loop_inode);
  deviceput(loop_dev);
  mntput(parent);
  res = 0;

exit:
  end_op();

  return res;
}
