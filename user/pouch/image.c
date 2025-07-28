// Pouch image management functions.

#include "image.h"

#include "fcntl.h"
#include "fsdefs.h"
#include "param.h"
#include "pouch.h"
#include "stat.h"
#include "util.h"

pouch_status pouch_images_print() {
  char full_path[MAX_PATH_LENGTH], *p;
  char dir[DIRSIZ + 1];
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(IMAGE_DIR, O_RDONLY)) < 0) {
    printf(stderr,
           "Cannot access the images dir, make sure the path %s exists\n",
           IMAGE_DIR);
    return ERROR_IMAGE_DIR_INVALID_CODE;
  }

  if (fstat(fd, &st) < 0) {
    printf(stderr,
           "cannot stat the images dir, make sure the path %s exists \n",
           IMAGE_DIR);
    close(fd);
    return ERROR_IMAGE_NOT_FOUND_CODE;
  }

  XV_Bool printed_first = XV_FALSE;
  if (st.type != T_DIR) {
    printf(stderr, "%s should be a directory\n", IMAGE_DIR);
    close(fd);
    return ERROR_IMAGE_INVALID_CODE;
  }

  strcpy(full_path, IMAGE_DIR);
  p = full_path + strlen(full_path);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (stat(full_path, &st) < 0) {
      printf(stdout, "Cannot stat %s\n", full_path);
      continue;
    }
    if (!fmtname(full_path, dir, sizeof(dir))) {
      printf(stdout, "path too long\n");
      continue;
    }
    if (strncmp(dir, ".", 1) != 0) {
      if (!printed_first) {
        printf(stdout, "Pouch images available:\n");
        printed_first = XV_TRUE;
      }
      printf(stdout, "%s\n", dir);
    }
  }

  if (!printed_first) {
    printf(stdout, "No images available.\n");
  }

  close(fd);
  return SUCCESS_CODE;
}

pouch_status pouch_image_exists(const char* const image_name) {
  char image_path[MAX_PATH_LENGTH];
  struct stat st;
  pouch_status status = SUCCESS_CODE;
  if ((status = pouch_image_get_path(image_name, image_path)) != SUCCESS_CODE) {
    goto end;
  }

  // Check if the image exists
  if (stat(image_path, &st) < 0) {
    status = ERROR_IMAGE_NOT_FOUND_CODE;
    goto end;
  }

end:
  return status;
}

pouch_status pouch_image_copy(const char* const source_image_name,
                              const char* const target_image_name) {
  char source_image_path[MAX_PATH_LENGTH], target_image_path[MAX_PATH_LENGTH];
  pouch_status status = SUCCESS_CODE;

  if (pouch_image_exists(source_image_name) != SUCCESS_CODE) {
    printf(stderr, "Source image %s does not exist.\n", source_image_name);
    status = ERROR_IMAGE_NOT_FOUND_CODE;
    goto end;
  }

  if (pouch_image_exists(target_image_name) == SUCCESS_CODE) {
    printf(stderr, "Target image %s already exists.\n", target_image_name);
    status = ERROR_IMAGE_ALREADY_EXISTS_CODE;
    goto end;
  }

  if ((status = pouch_image_get_path(source_image_name, source_image_path)) !=
      SUCCESS_CODE) {
    printf(stderr, "Failed to get path for source image %s.\n",
           source_image_name);
    goto end;
  }

  if ((status = pouch_image_get_path(target_image_name, target_image_path)) !=
      SUCCESS_CODE) {
    printf(stderr, "Failed to get path for target image %s\n",
           target_image_name);
    goto end;
  }

  if (cp(source_image_path, target_image_path) < 0) {
    status = ERROR_IMAGE_COPY_CODE;
    printf(stderr, "Failed to copy image %s to %s.\n", source_image_name,
           target_image_name);
    goto end;
  }

end:
  return status;
}

pouch_status pouch_image_rm(const char* const image_name) {
  char image_path[MAX_PATH_LENGTH];
  pouch_status status = SUCCESS_CODE;

  if (pouch_image_exists(image_name) != SUCCESS_CODE) {
    printf(stderr, "Image %s does not exist\n", image_name);
    status = ERROR_IMAGE_NOT_FOUND_CODE;
    goto end;
  }

  if ((status = pouch_image_get_path(image_name, image_path)) != SUCCESS_CODE) {
    printf(stderr, "Failed to get path for image %s\n", image_name);
    goto end;
  }

  if (unlink(image_path) < 0) {
    status = ERROR_IMAGE_REMOVE_CODE;
    printf(stderr, "Failed to remove image %s\n", image_name);
    goto end;
  }

end:
  return status;
}

pouch_status pouch_image_get_path(const char* image_name, char* image_path) {
  if (strlen(image_name) + strlen(IMAGE_DIR) > MAX_PATH_LENGTH) {
    perror("Image name is too long");
    return ERROR_IMAGE_NAME_TOO_LONG_CODE;
  }
  strcpy(image_path, IMAGE_DIR);
  strcat(image_path, image_name);
  return SUCCESS_CODE;
}
