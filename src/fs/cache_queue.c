#include <fs/cache_queue.h>
#include <core/arena.h>
#include <core/physical_memory.h>
#include <common/spinlock.h>

extern bool cache_debug;

static ListNode *head;     // the list of all allocated in-memory block.
static Arena arena;       // memory pool for `Block` struct.
static SpinLock lock;
/*
 * Here we got 3 functions operating on cache list:
 * 1) `insert_cache`
 * 2) `remove_cache`
 * 3) `get_cache`
 * 4) `scavenger`
 * 
 * insert into cache queue. 
 */
void 
insert_cache() {

}

/* 
 * remove from cache queue. 
 */
void 
remove_cache(Block *blk) {

}

/*
 * try getting from cache queue.  
 */
Block 
*get_cache(usize block_no) {

}

/*
 * clear unused cached blocks.
 */
void static 
scavenger() {
    usize cached_blocks_num;
    usize freed_slots_num;

    if (cache_debug) {
        printf("\n\
        [block scavenger] in cache block : %d(%x)\
        \n"
        , get_num_cached_blocks());
    }

    if (cache_debug) {
        printf("\n\
        [block scavenger] cleared : %d(%x)\
        \n"
        , get_num_cached_blocks());
    }
}

/*
 * init cache list 
 */
void 
init_cache_list() {
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(Block), allocator);
    init_spinlock(&lock, "cache_lock");
}