#include <fs/cache_queue.h>
#include <core/arena.h>
#include <core/physical_memory.h>
#include <common/spinlock.h>
#include <common/list.h>

static ListNode *head = NULL;     // the list of all allocated in-memory block.
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
static void 
insert_cache(Block *blk) {
    acquire_spinlock(&lock);
    ListNode *node = &blk->node;
    merge_list(head->prev, node);
    head = node;
    release_spinlock(&lock);
}

/* 
 * remove from cache queue. 
 */
static void 
remove_cache(Block *blk) {
    acquire_spinlock(&lock);
    ListNode *node = &blk->node;
    if (head == node) {
        if (head->next == head) 
            head = NULL;
        else
            head = head->next;
    }
    detach_from_list(node);
    release_spinlock(&lock);    
}

/*
 * try getting from cache queue.  
 */
Block *
get_cache(usize block_no) {
    acquire_spinlock(&lock);
    Block *blk = NULL;
    Block *res = NULL;
    ListNode *node = head;
    
    if (head != NULL) {
        blk = (Block *)node2blk(head);
        if (blk->block_no == block_no) {
            res = blk;
        }
        node = head->next;
        while(node != head && res == NULL) {
            blk = node2blk(node);
            if (blk->block_no == block_no) {
                res = blk;
            }
            node = node->next;
        }
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
    head = NULL;
}
