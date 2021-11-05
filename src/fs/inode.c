#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <fs/inode.h>

// this lock mainly prevents concurrent access to inode list `head`, reference
// count increment and decrement.
static SpinLock lock;
static ListNode head;

static const SuperBlock *sblock;
static const BlockCache *cache;
static Arena arena;

// find inode in list.
// NOTED : caller MUST hold the lock of the list !!!
static INLINE Inode *get_inode_inlist(usize inode_no) {
    ListNode *head_pointer = (ListNode*) &head;
    ListNode *current = head_pointer->next;
    Inode *inode = NULL;
    while(current != head_pointer) {
        inode = (Inode *) current;
        // corresponding inode is found in list.
        if (inode->inode_no == inode_no)
            return inode;
    }
    return NULL;
}

// return which block `inode_no` lives on.
// `inode_no` is the inode number.
// it only return where the block the inode lives on, to access where the inodeEntry is, use `get_entry` method.
static INLINE usize to_block_no(usize inode_no) {
    return sblock->inode_start + (inode_no / (INODE_PER_BLOCK));
}

// return the pointer to on-disk inode.
// worth noted that `inode_no` == 0 means the entry is unvalid, so corresponding 
// area on disk will never be used in the future. 
// for a valid inode, its inode number always LESS THAN (sblock -> num_inodes) and GREATER THAN 0.
static INLINE InodeEntry *get_entry(Block *block, usize inode_no) {
    return ((InodeEntry *)block->data) + (inode_no % INODE_PER_BLOCK);
}

// return pointer of indirect block, a.k.a INDIRECT_BLOCK array.
static INLINE u32 *get_addrs(Block *block) {
    return ((IndirectBlock *)block->data)->addrs;
}

// initialize inode tree.
// 
void init_inodes(const SuperBlock *_sblock, const BlockCache *_cache) {
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};

    init_spinlock(&lock, "InodeTree");
    init_list_node(&head);
    sblock = _sblock;
    cache = _cache;
    init_arena(&arena, sizeof(Inode), allocator);

    inodes.root = inodes.get(ROOT_INODE_NO);
}

// initialize in-memory inode.
static void init_inode(Inode *inode) {
    init_spinlock(&inode->lock, "Inode");
    init_rc(&inode->rc);
    init_list_node(&inode->node);
    inode->inode_no = 0;
    inode->valid = false;
}

//
// to check if inode on disk is valid, check INODE_TYPE if equal to INODE_INVALIED.
// how to acquire a block? answer is using `acquire` provided by `BlockCache`.
// to address which block the inode lives on, use `to_block_no` method.
// to know where the entry is, use `get_entry` method.
// and then we can know the inode's type, and if it's available for allcocating new inode.
// inode in memory is always newest copy of inode on disk, waiting to be written back to disk.
//
// 1. acquire block.
// 2. read inodeEntry.
// 3. check if inodeEntry is valid.
// 4. if unvalid, choose the entry, set it to valid. 
// 5. make a copy in memory, return corresponding inode number.
// 5. otherwise if all entry has been used, call PANIC.
// see `inode.h`.
// 
static usize inode_alloc(OpContext *ctx, InodeType type) {
    assert(type != INODE_INVALID);

    usize i = 0, j;
    usize inode_number = 0, block_number, check_number;
    // begin atomic operation.
    cache->begin_op(ctx);
    for(;; i += INODE_PER_BLOCK) {
        if (i >= sblock->num_inodes) break;
        block_number = to_block_no(i);
        // acquire block.
        Block * block = cache->acquire(block_number);

        for(j = 0; j < INODE_PER_BLOCK; j++) {
            check_number = i+j;
            if (check_number > 0 && check_number < sblock->num_inodes) {
                InodeEntry * entry = get_entry(block, check_number);
                if (entry->type == INODE_INVALID) {
                    inode_number = check_number;
                    // make it valid.
                    entry->type = INODE_REGULAR;
                    cache->sync(ctx, block);
                    // apply and initialize new inode in memory.
                    // apply -> init -> merge_list.
                    // does it needed in this function?
                    // TODO : if we need to initialize new inode in memory?
                    break;
                }
            }
        }
        // release block.
        cache->release(block);
        // inode is found.
        if (inode_number) break;
    }
    // end atomic operation on sd.
    cache->end_op(ctx);

    if (inode_number) return inode_number;
    else PANIC("failed to allocate inode on disk");
}

// see `inode.h`.
static void inode_lock(Inode *inode) {
    assert(inode->rc.count > 0);
    acquire_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_unlock(Inode *inode) {
    assert(holding_spinlock(&inode->lock));
    assert(inode->rc.count > 0);
    release_spinlock(&inode->lock);
}

// 
// `inode.h`.
// we know that, inode has a newest copy in memory, and we need to write it back to disk.
// 0. must not aquire `inode -> lock`, caller holds it.
// 1. if `do_write` == True, write entry back to disk.
// 2. else if `do_write` == False, and inodeEntry is unvalid, write from disk to memory.
// 
static void inode_sync(OpContext *ctx, Inode *inode, bool do_write) {
    // some initliazation and neccessary condition check.
    InodeEntry *im_entry = &inode->entry;
    usize inode_number = inode->inode_no;
    usize block_number = to_block_no(inode_number);
    cache->begin_op(ctx);
    Block *block = cache->acquire(block_number);
    InodeEntry *sd_entry = get_entry(block, inode_number);
    // if inode on disk is unvalid.
    if (inode_number <= 0 || inode_number >= sblock->num_inodes)
        PANIC("inode_sync : inode_number <= 0 or inode_number >= MAX_num_inodes.\n");
    if (sd_entry->type == INODE_INVALID) 
        PANIC("inode_sync : trying to synchronize unvalid inode on disk.\n");
    
    // copy and synchronization.
    if (inode->valid == false) {
        // copy entry on disk to in-memory inode.
        memcpy(im_entry, sd_entry, sizeof(InodeEntry));
    } else if (inode->valid == true && do_write) {
        // synchronize data to disk.
        memcpy(sd_entry, im_entry, sizeof(InodeEntry));
        cache->sync(ctx, block);
    } else { 
        // else PANIC. 
        PANIC("inode_sync : inode true and do_write == false.\n");
    }
    // do not forget to release the block.
    cache->release(block);
    // do not forget to end atomic operation.
    cache->end_op(ctx);
}

//
// see `inode.h`.
// 0. Holding `inode -> lock` and `list -> lock`.
// 1. find corresponding inode in memory.
// 2. if no found in list or inode is invalid, try to load it from disk.
// 3. if corresponding inode is unvalid, return NULL to caller.
// 4. `inode_get` will increase the reference count of inode.
//
static Inode *inode_get(usize inode_no) {
    assert(inode_no > 0);
    assert(inode_no < sblock->num_inodes);

    // try to find the inode in list first.
    Inode *inode = NULL;
    acquire_spinlock(&lock);
    inode = get_inode_inlist(inode_no);
    release_spinlock(&lock);

    // if inode has been in list.
    if (inode != NULL) {
        increment_rc(&inode->rc);
    }

    // otherwise inode not in list. allocate and initliaze a new one.
    else{
        inode = (Inode *)alloc_object(&arena);
        // PANIC if arena fail to allocate new Inode.
        if (inode == NULL) 
            PANIC("inode_get : arena fails to allocate new inode.\n");
        // do some normal initialization, DO NOT FORGET to LOCK object !!!
        init_inode(inode);
        // aquire inode lock since we edit values on it && no caller holds the lock.
        acquire_spinlock(&inode->lock);
        inode->inode_no = inode_no;
        increment_rc(&inode->rc);
        release_spinlock(&inode->lock);
        // aquire list lock since we want to edit the list.
        acquire_spinlock(&lock);
        merge_list(&head, &inode->node);
        release_spinlock(&lock);
    }

    // final check.
    if (inode == NULL) 
        PANIC("inode_get : something unusual happens.\n");
    return inode;
}

//
// see `inode.h`.
// 0. Holding `inode -> lock` and `list -> lock`.
// 1. delete corresponding inode on disk (make it unvalid).
// 2. free corresponding inode in memory (delete it from list using ``).
// 3. how to clear the data blocks on disk?
// 
static void inode_clear(OpContext *ctx, Inode *inode) {
    InodeEntry *entry = &inode->entry;

    // TODO
    // clear the inode in memory.
    // write the inode to disk, using method `inode_sync`.

}

//
// see `inode.h`.
// 0. Holding no lock.
//
static Inode *inode_share(Inode *inode) {
    acquire_spinlock(&lock);
    increment_rc(&inode->rc);
    release_spinlock(&lock);
    return inode;
}

//
// see `inode.h`.
// 0. Holding `inode -> lock`.
// 1. decrease the reference count of inode.
// 2. if reference count is zero, clear the inode, call `inode_clear`.
//
static void inode_put(OpContext *ctx, Inode *inode) {
    // TODO
    
}

// this function is private to inode layer, because it can allocate block
// at arbitrary offset, which breaks the usual file abstraction.
//
// retrieve the block in `inode` where offset lives. If the block is not
// allocated, `inode_map` will allocate a new block and update `inode`, at
// which time, `*modified` will be set to true.
// the block number is returned.
//
// NOTE: caller must hold the lock of `inode`.
//
// 0. caller holding lock, so `inode_map` does not hold any lock. Assuming offset means position of byte (counting from 0).
// 1. check if offset is over or equal to INODE_MAX_LENGTH.
// 2. check if offset byte in direct block.
// 3. check if offset byte in indirect block.
// 4. if correspongding block is not allocated, allocate it.
static usize inode_map(OpContext *ctx, Inode *inode, usize offset, bool *modified) {
    InodeEntry *entry = &inode->entry;

    // TODO


    return 0;
}

// see `inode.h`.
// 1. call cache's `acquire` to access the block.
// 2. copy corresponding data from block to dest.
// 3. do forget to `release` the block every time call `acquire`.
static void inode_read(Inode *inode, u8 *dest, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= entry->num_bytes);
    assert(offset <= end);

    // TODO
}

// see `inode.h`.
// 1. call cache's `acquire` to access the block.
// 2. copy corresponding data from `src` to block.
// 3. synchronize data to disk.
// 3. do forget to `release` the block every time call `acquire`.
static void inode_write(OpContext *ctx, Inode *inode, u8 *src, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= INODE_MAX_BYTES);
    assert(offset <= end);

    // TODO
}

//
// see `inode.h`.
// caller holding the lock so `inode_lookup` does not hold any lock.
// 1. check if type of inode is INODE_DIR.
// 2. check if `name` is empty.
// 3. check if `name` is too long.
// 4. check if `name` in this directory.
// 
static usize inode_lookup(Inode *inode, const char *name, usize *index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // TODO

    return 0;
}

//
// see `inode.h`.
// caller holding the lock so `inode_lookup` does not hold any lock.
// 1. check if type of inode is INODE_DIR.
// 2. check if space is enough for new entry.
// 3. check if `name` is too long.
// 4. after insertion, update inode's num_bytes.
// 5. return index of entry on the inode.
// 
static usize inode_insert(OpContext *ctx, Inode *inode, const char *name, usize inode_no) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // TODO

    return 0;
}

// see `inode.h`.
static void inode_remove(OpContext *ctx, Inode *inode, usize index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);

    // TODO
}

InodeTree inodes = {
    .alloc = inode_alloc,
    .lock = inode_lock,
    .unlock = inode_unlock,
    .sync = inode_sync,
    .get = inode_get,
    .clear = inode_clear,
    .share = inode_share,
    .put = inode_put,
    .read = inode_read,
    .write = inode_write,
    .lookup = inode_lookup,
    .insert = inode_insert,
    .remove = inode_remove,
};
