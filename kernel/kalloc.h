#ifndef XV6_KALLOC_H
#define XV6_KALLOC_H

typedef struct kmemtest_info {
  int page_cnt;
  int list_cnt;
  int err_cnt;
} kmemtest_info;

#endif /* XV6_KALLOC_H */
