#include <fs/cache_queue.h>
#include <core/arena.h>
#include <core/physical_memory.h>
#include <common/spinlock.h>
#include <common/list.h>

static ListNode head;     // the list of all allocated in-memory block.
static SpinLock lock;

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
    acquire_spinlock(&lock);
    ListNode *node = &blk->node;
    merge_list(&head.prev, node);
    release_spinlock(&lock);
}

/* 
 * remove from cache queue. 
 * caller hold the cache queue lock.
 */
static void 
remove_cache(Block *blk) {
    // acquire_spinlock(&lock);
    ListNode *node = &blk->node;
    detach_from_list(node);
    // release_spinlock(&lock);    
}

/*
 * try getting from cache queue.  
 */
Block *
get_cache(usize block_no) {
    acquire_spinlock(&lock);
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
    
    release_spinlock(&lock);
    return res;
}

/*
 * init cache queue's lock and head pointer. 
 */
void 
init_cache_list() {
    init_spinlock(&lock, __FILE__);
    init_list_node(&head);
}


/*
 * clear unused cached blocks.
 * caller must hold the cache lock in cache.c
 */
void 
scavenger() {
    usize cached_blocks_num = get_num_cached_blocks();

    if (cache_debug()) {
        printf("\n\
=> [block scavenger] in cache block : %d blocks\
        \n"
        , cached_blocks_num);
    }

    acquire_spinlock(&lock);
    ListNode *node = head.next;
    while(node != &head) {
        Block *blk = (Block *)node2blk(node);
        if (blk->acquired) {
            remove_cache(blk);
            exile_cache(blk);
        }
    }
    release_spinlock(&lock);

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
