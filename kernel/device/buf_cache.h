#ifndef XV6_DEVICE_BUF_CACHE_H
#define XV6_DEVICE_BUF_CACHE_H

#include "defs.h"

void buf_cache_init();
void buf_cache_invalidate_blocks(const struct device*);
struct buf* buf_cache_get(const struct device*, const union buf_id*, uint);
void buf_cache_release(struct buf* b);
uint buf_cache_is_cache_enabled(void);
void buf_cache_enable_cache(void);
void buf_cache_disable_cache(void);

#endif  // XV6_DEVICE_BUF_CACHE_H
