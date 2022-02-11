#include <fs/cache_queue.h>
#include <core/arena.h>
#include <core/physical_memory.h>
#include <common/spinlock.h>
#include <common/list.h>

static ListNode head;     // the list of all allocated in-memory block.
static SpinLock qlock;

/*
 * 3 functions operating on cache list:
 * 1) `insert_cache`
 * 2) `remove_cache`
 * 3) `get_cache`
 * 4) `scavenger`
 * 
 * insert into cache queue. 
 */
void 
insert_cache(Block *blk) {
    acquire_spinlock(&qlock);
    ListNode *node = &blk->node;
    merge_list(&head.prev, node);
    release_spinlock(&qlock);
}

/* 
 * remove from cache queue. 
 * caller hold the cache queue qlock.
 */
static void 
remove_cache(Block *blk) {
    // acquire_spinlock(&qlock);
    ListNode *node = &blk->node;
    detach_from_list(node);
    // release_spinlock(&qlock);    
}

/*
 * try getting from cache queue.  
 */
Block *
get_cache(usize block_no) {
    acquire_spinlock(&qlock);
    Block *blk = NULL;
    Block *res = NULL;
    ListNode *node = head.next;
    
    while(node != &head && res == NULL) {
        blk = node2blk(node);
        if (blk->block_no == block_no) {
            res = blk;
        }
        node = node->next;
    }
    
    release_spinlock(&qlock);
    return res;
}

/*
 * init cache queue's qlock and head pointer. 
 */
void 
init_cache_list() {
    init_spinlock(&qlock, __FILE__);
    init_list_node(&head);
}


/*
 * clear unused cached blocks.
 * caller must hold the cache qlock in cache.c
 */
void 
scavenger() {
    usize cached_blocks_num = get_num_cached_blocks();

    if (cache_debug()) {
        printf("\n\
=> \033[42;37;5m[block scavenger]\033[0m: in cache block : %d blocks\
        \n"
        , cached_blocks_num);
    }

    acquire_spinlock(&qlock);
    ListNode *node = head.next;
    while(node != &head && get_num_cached_blocks() > EVICTION_THRESHOLD) {
        Block *blk = (Block *)node2blk(node);
        node = node->next;
        if (blk->acquired == false && blk->pinned == false) {
            remove_cache(blk);
            exile_cache(blk);
        }
    }
    release_spinlock(&qlock);

    usize new_cached_blocks_num = get_num_cached_blocks();
    usize freed_slots_num = cached_blocks_num - new_cached_blocks_num;

    if (cache_debug()) {
        printf("\n\
                      after clearing : %d blocks\n\
                      clear %d cached blocks \n\
        \n"
        , new_cached_blocks_num, freed_slots_num);
    }
}
