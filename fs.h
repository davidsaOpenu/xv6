#pragma once

#include "file.h"
#include "stat.h"

// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOTINO 1  // root i-number

#define MAX_INODE_OBJECT_DATA (1ull << 25)  // 32MB

/**
 * On-disk inode structure
 * Currently, all the inode data is located inside a single object. We can
 * improve the implementation by implementing an indirect structure as in the
 * original xv6 file system.
 *
 * Inodes object names are "inode" and then the object id as 4 bytes. Because
 * part of the bytes can be zero we must change them so the id won't be cut in
 * the middle. Hence, we set the highest bit on for all the bytes and don't
 * use it to store the id. Hence, each byte contains only 127 possible values
 * and not 255. To make up for the loss, we add another byte to the id.
 For example, "inode5" would be "inode\x85\x80\x80\x80\x80" for uint of size 4
 * bytes. 
 *
 * Not like in the original xv6 implementation, here we have no limitation on
 * the amount of inodes and we don't preserve space for them. This is done by
 * the fact the inodes are regular objects.
 * Moreover the `dinode` doesn't contain the inode name becuse it is the name
 * of the object.
 */
struct dinode {
  short type;  // File type
  short major; // Major device number (T_DEV only)
  short minor; // Minor device number (T_DEV only)
  short nlink; // Number of links to inode in file system
  // the object containning the data of this inode
  char data_object_name[MAX_OBJECT_NAME_LENGTH];
};

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

/**
 * 5 is `strlen("inode")`
 * sizeof(uint) + 1 is the size of the counter
 * 1 for null terminator
 */
#define INODE_NAME_LENGTH (5 + sizeof(uint) + 1 + 1)

int             dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(uint, short);
struct inode*   idup(struct inode*);
void            iinit(int dev);
void            ilock(struct inode*);
void            inode_name(char* output, uint inum);
void            iput(struct inode*);
struct inode*   iget(uint dev, uint inum);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, char*, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, const char*, uint, uint);
