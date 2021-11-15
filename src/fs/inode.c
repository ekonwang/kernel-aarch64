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
        current = current->next;
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
// 0. Must no hold any lock of inode or list.
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
    
    for(;; i += INODE_PER_BLOCK) {
        if (i >= sblock->num_inodes) break;
        
        // find the right block.
        block_number = to_block_no(i);
        
        Block * block = cache->acquire(block_number);
        for(j = 0; j < INODE_PER_BLOCK; j++) {
            check_number = i+j;
            if (check_number > 1 && check_number < sblock->num_inodes) {
                InodeEntry * entry = get_entry(block, check_number);
                if (entry->type == INODE_INVALID) {
                    inode_number = check_number;
                    memset(entry, 0, sizeof(InodeEntry));
                    entry->type = type;
                    cache->sync(ctx, block);
                    break;
                }
            }
        }
        cache->release(block);

        if (inode_number) break;
    }

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
        // now in-memory inode is valid.
        memcpy(im_entry, sd_entry, sizeof(InodeEntry));
        inode->valid = true;
    } else if (inode->valid == true && do_write) {
        // synchronize data to disk.
        memcpy(sd_entry, im_entry, sizeof(InodeEntry));
        cache->sync(ctx, block);
    } else { 
        // else do nothing. 
    }

    // do not forget to release the block.
    cache->release(block);
}

//
// see `inode.h`.
// 0. Need hold `inode -> lock` and `list -> lock`.
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
        // allocate object using memory pool.
        inode = (Inode *)alloc_object(&arena);
        // PANIC if arena fail to allocate new Inode.
        if (inode == NULL) 
            PANIC("inode_get : arena fails to allocate new inode.\n");

        // do some normal initialization, DO NOT FORGET to LOCK object !!!
        init_inode(inode);

        // aquire inode lock since we edit values on it && no caller holds the lock.
        acquire_spinlock(&inode->lock);

        // inode_no & reference count edit.
        inode->inode_no = inode_no;
        inode_sync(NULL, inode, false);

        // when inode_no == root_dir == 1
        if (inode_no == 1) {
            inode->entry.type = INODE_DIRECTORY;
        }

        increment_rc(&inode->rc);

        // aquire list lock since we want to edit the list.
        acquire_spinlock(&lock);
        merge_list(&head, &inode->node);
        release_spinlock(&lock);


        // release lock of inode.
        release_spinlock(&inode->lock);
    }

    // final check.
    if (inode == NULL) 
        PANIC("inode_get : something unusual happens.\n");
    return inode;
}

//
// see `inode.h`.
// 0. MUST NOT Hold `inode -> lock`, NEED aquire `list -> lock`.
// 1. delete corresponding inode on disk (make it unvalid).
// 2. free corresponding inode in memory (delete it from list using ``).
// 3. how to clear the data blocks on disk?
// 
static void inode_clear(OpContext *ctx, Inode *inode) {
    InodeEntry *entry = &inode->entry;

    // initialization.
    InodeEntry *im_entry = &inode->entry;
    u32 *addrs = &im_entry->addrs;
    usize i = 0;

    // try to free all data blocks on entry.
    // first try to free the direct data block.
    for(; i<INODE_NUM_DIRECT; i++) {
        // if there is allocated data block, free it.
        if (addrs[i]) {
            if (inode->valid == true)
                cache->free(ctx, addrs[i]);
            addrs[i] = (u32)0;
        }
    }

    // second try to free the indirect data block.
    if (im_entry->indirect && inode->valid == true) {
        Block *block = cache->acquire(im_entry->indirect);
        for (i=0; i<INODE_NUM_INDIRECT; i++) {
            u32 *block_no_addr = (u32 *)block + i;

            // if there is allocated data block in indirect block, free it.
            if (*block_no_addr) {
                cache->free(ctx, *block_no_addr);
            }
        }
        
        // unlock the indirect block and free it.
        cache->release(block);
        cache->free(ctx, im_entry->indirect);
    }
    entry->indirect = (u32)0;

    // now all contents has been discard.

    // finally synchronize entry to sd.
    inode_sync(ctx, inode, true);
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
    // aquire inode's lock
    acquire_spinlock(&inode->lock);

    // decrease the reference count of inode.
    decrement_rc(&inode->rc);

    // if ref number > 0, return.
    if (inode->rc.count) {
        release_spinlock(&inode->lock);
        return;
    }

    // if entry.num_links == 0, destroy inode both in memory and disk.
    if (inode->entry.num_links == 0 && inode->inode_no != 1) {
        // free data block of the inode first.
        inode_clear(ctx, inode);

        // make it unvalid (all-zero).
        memset(&inode->entry, 0, sizeof(InodeEntry));

        // sync to disk.
        inode_sync(ctx, inode, true);
    }

    // if entry.num_links > 0, only destroy inode in memory.
    acquire_spinlock(&lock);
    detach_from_list(&inode->node);
    release_spinlock(&lock);
    
    release_spinlock(&inode->lock);

    free_object(inode);
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
// 0. caller holding lock, so `inode_map` does not hold inode lock. 
//    `inode_map` does not hold atomic lock (`begin_op` and `end_op`).
//    its caller `inode_write` holds the atomic operation lock.
// 1. check if offset is over or equal to INODE_MAX_LENGTH.
// 2. check if offset byte in direct block.
// 3. check if offset byte in indirect block.
// 4. if correspongding block is not allocated, allocate it.
// 
static usize inode_map(OpContext *ctx, Inode *inode, usize offset, bool *modified) {
    InodeEntry *entry = &inode->entry;
    assert(offset < INODE_MAX_BYTES);
    usize block_index = offset / BLOCK_SIZE, block_no = 0;
    u32 *block_entry = NULL;

    // if block_index is at direct area.
    if (block_index < INODE_NUM_DIRECT) {
        block_entry = &entry->addrs[block_index];
    }

    // else if block_index is at indirect area.
    else {
        block_index -= INODE_NUM_DIRECT;

        // if indirect entry is not used, initialize indirect block.
        if (entry->indirect==0) {
            entry->indirect = cache->alloc(ctx);
            *modified = true;
        }

        // find corresponding entry position.
        IndirectBlock *block = (IndirectBlock *)cache->acquire(entry->indirect);
        block_entry = &block->addrs[block_index];
        cache->release(block);
    }

    // if entry is allocated before.
    if (*block_entry) 
        block_no = *block_entry;

    // if this entry is not allocated.
    else {
        *block_entry = cache->alloc(ctx);
        block_no = *block_entry;
        *modified = true;
    }

    return block_no;
}

// helper function for `inode_read`.
static usize inode_map2(Inode *inode, usize offset) {
    InodeEntry *entry = &inode->entry;
    assert(offset < INODE_MAX_BYTES);
    usize block_index = offset / BLOCK_SIZE, block_no = 0;
    
    // if block_index is at direct area.
    if (block_index < INODE_NUM_DIRECT) {
        assert(entry->addrs[block_index]);
        return entry->addrs[block_index];
    }

    // else if block_index is at indirect area.
    else {
        block_index -= INODE_NUM_DIRECT;
        assert(entry->indirect);
        IndirectBlock *block = (IndirectBlock *)entry->indirect;
        assert(block->addrs[block_index]);
        return block->addrs[block_index];
    }
}

// see `inode.h`.
// 0. not holding lock of inode.
// 1. call cache's `acquire` to access the block.
// 2. copy corresponding data from block to dest.
// 3. do forget to `release` the block every time call `acquire`.
static void inode_read(Inode *inode, u8 *dest, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset < entry->num_bytes);
    assert(end <= entry->num_bytes);
    assert(offset <= end);

    usize i = round_down(offset, BLOCK_SIZE);
    usize start, term;
    while(i < end) {
        // i: data block_no.
        // copydata start from start, ends at term in this block.
        start = MAX(i, offset)-i;
        term = MIN(i+BLOCK_SIZE, end)-i;
        // begin copy.
        usize block_no = inode_map2(inode, i);
        Block *block = cache->acquire(block_no);
        memcpy(dest, (u8 *)block + start, term-start);
        cache->release(block);
        // update.
        dest += term-start;
        i += BLOCK_SIZE;
    }
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

    // define some values.
    bool modify = false;
    usize i = round_down(offset, BLOCK_SIZE);
    usize start, term;

    // begin writing.
    while(i <= end) {
        // i: data block_no.
        // copydata start from start, ends at term in this block.
        start = MAX(i, offset)-i;
        term = MIN(i+BLOCK_SIZE, end)-i;

        // begin mapping (may change inode on disk).
        usize block_no = inode_map(ctx, inode, i, &modify);
        
        // begin copying to disk.
        Block *block = cache->acquire(block_no);
        memcpy((u8 *)block + start, src, term-start);
        cache->release(block);

        // update terminal and start.
        src += term-start;
        i += BLOCK_SIZE;
    }
    
    // update number of bytes of the entry.
    if (modify || entry->num_bytes < end) {
        entry->num_bytes = MAX(end, entry->num_bytes);
        inode_sync(ctx, inode, true);
    }
}

//
// see `inode.h`.
// caller holding the lock so `inode_lookup` does not hold any lock.
// 1. check if type of inode is INODE_DIR.
// 2. find the position of the entry with name.
// 3. if no found, return 0 indicating no entry matches with it.
// 
static usize inode_lookup(Inode *inode, const char *name, usize *index) {
    InodeEntry *entry = &inode->entry;
    
    // make sure data is stored uniformly in INODE_DIR inode.
    // make sure name is not so long.
    assert(entry->type == INODE_DIRECTORY);
    assert(entry->num_bytes % sizeof(DirEntry) == 0);
    assert(strlen(name) < FILE_NAME_MAX_LENGTH);
    
    // definitions of some values.
    usize len = 0;
    usize block_index = 0;
    usize name_length = strlen(name);
    char buffer[BLOCK_SIZE];

    // begin matching.
    while(block_index <= entry->num_bytes) {
        // len : how long content need to get from this block?
        len = MIN(block_index+BLOCK_SIZE, entry->num_bytes) - block_index;
        
        // call `inode_read` to access the content.
        inode_read(inode, buffer, block_index, len);
        
        // now the buffer has the copy of content of block.
        // look up in the buffer :
        for (usize in_block = 0; in_block < len; in_block += sizeof(DirEntry)) {
            assert(in_block+block_index < entry->num_bytes);
            DirEntry *dir_entry = (DirEntry *)(buffer + in_block);
            char *this_name = dir_entry->name;

            // '\0' position must matches. if matches.
            if (!memcmp(name, this_name, name_length+1)) {
                
                // `*index` assigned with direntry index.
                // return inode_no.
                // noted that inode_lookup only return FIRST match.
                *index = block_index + in_block;
                return dir_entry->inode_no;
            }
        }
        
        // update block_index
        block_index += BLOCK_SIZE;
    }

    // if no match found, return zero to caller.
    return 0;
}

//
// see `inode.h`.
// 0. caller holding the lock so `inode_lookup` does not hold any lock.
// 1. find corresponding position for new entry.
// 2. copy and increase num_bytes of dir inode entry.
// 3. increase num_links of the corresponding inode.
// 
static usize inode_insert(OpContext *ctx, Inode *inode, const char *name, usize inode_no) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);
    assert(entry->num_bytes % sizeof(DirEntry) == 0);
    assert(entry->num_bytes < INODE_MAX_BYTES);
    usize numbytes = entry->num_bytes;

    
    // initialize an entry.
    DirEntry dir_entry;
    dir_entry.inode_no = inode_no;
    strncpy(&dir_entry.name, name, FILE_NAME_MAX_LENGTH);
    
    // write.
    inode_write(ctx, inode, (u8 *)&dir_entry, numbytes, sizeof(DirEntry));

    return numbytes;
}

// see `inode.h`.
// 0. `inode_remove` aquire no lock of the inode.
// 1. find the entry.
// 2. remove entry in dir inode cause opposite effects on inode entry with `inode_insert`.
// 3. reduce num_links of the corresponding inode.
//
static void inode_remove(OpContext *ctx, Inode *inode, usize index) {
    InodeEntry *entry = &inode->entry;
    // uniform check.
    assert(entry->type == INODE_DIRECTORY);
    assert(index < INODE_MAX_BYTES);
    assert(index % sizeof(DirEntry) == 0);
    
    // the area of the inode has not been used yet.
    if (index >= entry->num_bytes) 
        return;

    // begin write all-zero to corresponding area.
    DirEntry dir_entry;
    memset(&dir_entry, 0, sizeof(DirEntry));
    inode_write(ctx, inode, (u8 *)&dir_entry, index, sizeof(DirEntry));

    // if neccesary, edit num_bytes.
    if (index + sizeof(DirEntry) == entry->num_bytes)
        entry->num_bytes = index;
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
