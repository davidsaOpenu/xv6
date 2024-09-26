#ifndef XV6_TYPES_H
#define XV6_TYPES_H

typedef enum { false, true } bool;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char uchar;
typedef uint pde_t;

#ifndef NULL
#define NULL 0
#endif

#define offsetof(TYPE, MEMBER) \
  ((unsigned int)(&((TYPE *)0)->MEMBER))  // NOLINT(runtime/casting)

#define container_of(ptr, type, member)                \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })

#endif /* XV6_TYPES_H */
