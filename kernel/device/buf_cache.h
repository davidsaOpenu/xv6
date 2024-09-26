#ifndef XV6_DEVICE_BUF_CACHE_H
#define XV6_DEVICE_BUF_CACHE_H

#include "defs.h"

void buf_cache_init();
void buf_cache_invalidate_blocks(const struct device*);
struct buf* buf_cache_get(const struct device*, const union buf_id*, uint);
void buf_cache_release(struct buf* b);

#endif  // XV6_DEVICE_BUF_CACHE_H
