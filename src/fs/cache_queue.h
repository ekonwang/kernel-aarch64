#pragma once

#include <fs/cache.h>
#include <fs/defines.h>

#define node2blk(mptr) container_of(mptr, Block, node)

void insert_cache();
Block *get_cache(usize block_no);
void init_cache_list();
void scavenger();