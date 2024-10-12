#ifndef XV6_USER_POUCH_IMAGE_H
#define XV6_USER_POUCH_IMAGE_H

#include "pouch.h"

#define IMG_NAME_SIZE (100)

/*
 *   Get all avaliable images
 *   @input: none
 *   @output: prints all avaliable images
 */
pouch_status pouch_images_print();

/*
 *   Check if image exists in images list by it's name
 *   @input: image_name
 *   @output: none
 */
pouch_status pouch_image_exists(const char* const image_name);

/*
 *   Prepare image name to path:
 *   - Create a path in cgroup fs for corresponding cname
 *   @input: image_name
 *   @output: image_path
 */
pouch_status pouch_image_get_path(const char* image_name, char* image_path);

#endif  // XV6_USER_POUCH_IMAGE_H
