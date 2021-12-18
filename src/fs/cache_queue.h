#include <fs/cache.h>

void insert_cache();
void remove_cache(Block *blk);
Block *get_cache(usize block_no);
void scavenger();
void init_cache_list();
