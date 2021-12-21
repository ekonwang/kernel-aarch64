#include <common/bitmap.h>
#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <fs/cache.h>
#include <fs/cache_queue.h>

static const SuperBlock *sblock;
static const BlockDevice *device;

bool static _cache_debug = true; // output info in debug mode.

static SpinLock lock;     // protects block cache.
static LogHeader header;  // in-memory copy of log header block.
static Arena arena;       // memory pool for `Block` struct.
static usize outstanding; // how many syscall is made.
static usize committing;  // if is commiting, if true, blocking new operation.

static void crash_recover();

// hint: you may need some other variables. Just add them here.

// read the content from disk.
static INLINE void device_read(Block *block) {
    device->read(block->block_no, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block *block) {
    device->write(block->block_no, block->data);
}

// read log header from disk.
static INLINE void read_header() {
    device->read(sblock->log_start, (u8 *)&header);
}

// write log header back to disk.
static INLINE void write_header() {
    device->write(sblock->log_start, (u8 *)&header);
}

const bool cache_debug() {
    return _cache_debug;
}

// initialize block cache.
void init_bcache(const SuperBlock *_sblock, const BlockDevice *_device) {
    sblock = _sblock;
    device = _device;

    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(Block), allocator);
    init_cache_list();
    init_spinlock(&lock, __FILE__);
    _cache_debug = true;

    // if neccessary, recover from crash.
    crash_recover();
}

// exile cached block from arena.
void exile_cache(Block *blk) {
    free_object(blk);
}


// initialize a block struct.
static void init_block(Block *block) {
    block->block_no = 0;
    init_list_node(&block->node);
    block->acquired = false;
    block->pinned = false;

    init_sleeplock(&block->lock, "block");
    block->valid = false;
    memset(block->data, 0, sizeof(block->data));
}

// load bitmap


// see `cache.h`.
usize get_num_cached_blocks() {
    return arena.num_objects;
}

// see `cache.h`.
static Block *cache_acquire(usize block_no) {
    Block *blk = get_cache(block_no);
    if (blk == NULL) {
        blk = (Block *)alloc_object(&arena);
        init_block(blk);
        insert_cache(blk);

        device->read(block_no, &blk->data);
        blk->valid = true;
    }
    acquire_sleeplock(&(blk->lock));

    if (get_num_cached_blocks() >= EVICTION_THRESHOLD) {
        scavenger();
    }
    return blk;
}

// see `cache.h`.
static void cache_release(Block *block) {
    release_sleeplock(&(block->lock));
}

// see `cache.h`.
static void cache_begin_op(OpContext *ctx) {
    while (committing) 
        sleep(&lock, NULL);
    while ((outstanding + 1) * OP_MAX_NUM_BLOCKS > LOG_MAX_SIZE) 
        sleep(&lock, NULL);
    outstanding += 1;
    ctx->ops_cnt = 0;
}

// see `cache.h`.
static void cache_sync(OpContext *ctx, Block *block) {
    acquire_spinlock(&lock);
    if (ctx) {
        ctx->ops_cnt += 1;
        if (ctx->ops_cnt > OP_MAX_NUM_BLOCKS) {
            PANIC("operation numbers exceed limit");
        }

        usize i;
        for (i = 0; i < header.num_blocks; i++) {
            if (header.block_no[i] == block->block_no) 
                break;
        }
        if (i == header.num_blocks) {
            header.block_no[i] = block->block_no;
            header.num_blocks += 1;
        }
        block->pinned = true;
    } else
        device_write(block);
    release_spinlock(&lock);
}

static void crash_recover() {
    usize i;
    SuperBlock *sb = get_super_block();
    usize start = sb->log_start; 
    u8 buffer[BLOCK_SIZE];

    read_header();
    for (i = 0; i < header.num_blocks; i++) {
        device->read(start + 1 + i, &(buffer));
        device->write(header.block_no[i], &(buffer));
    }
    header.num_blocks = 0;
    write_header();
}

static void commit() {
    Block *from;
    usize i;
    SuperBlock *sb = get_super_block();
    usize start = sb->log_start; 

    for (i = 0; i < header.num_blocks; i++) {
        from = get_cache(header.block_no[i]);
        device->write(start + 1 + i, &(from->data)); 
        // unpin the block cache.
        from -> pinned = false;
    }

    crash_recover();
}

// see `cache.h`.
static void cache_end_op(OpContext *ctx) {
    acquire_spinlock(&lock);
    if (outstanding == 1) {
        committing = 1;

        commit();
        committing = 0;
    } 

    wakeup(&lock);
    outstanding -= 1;
    release_spinlock(&lock);
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static usize cache_alloc(OpContext *ctx) {
    usize block_no = -1;
    usize blki, blkj;
    SuperBlock *sb = get_super_block();
    usize totblk = sb->num_blocks;
    usize bitmap_start = sb->bitmap_start;

    for (blki = 0; blki < totblk; blki += BIT_PER_BLOCK) {
        Block *block = cache_acquire(bitmap_start + blki/BIT_PER_BLOCK);
        for (blkj = 0; blkj < BIT_PER_BLOCK && blki + blkj < totblk; blkj++) {
            if (bitmap_get(&(block->data), blkj) == false) {

                bitmap_set(&(block->data), blkj);
                block_no = blki + blkj;
                cache_sync(ctx, block);
                cache_release(block);
                return block_no;
            }
        }
        cache_release(block);
    }
    
    PANIC("cache_alloc: no free block");
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void cache_free(OpContext *ctx, usize block_no) {
    usize blki = (block_no) / BIT_PER_BLOCK;
    usize blkj = (block_no) % BIT_PER_BLOCK;
    SuperBlock *sb = get_super_block();
    usize bitmap_start = sb->bitmap_start;
    Block *block = cache_acquire(bitmap_start + blki);

    if (bitmap_get(&(block->data), blkj) == false) {
        PANIC("cache_free: trying free a free block");
    }
    bitmap_clear(&(block->data), blkj);
    cache_sync(ctx, block);
}

BlockCache bcache = {
    .get_num_cached_blocks = get_num_cached_blocks,
    .acquire = cache_acquire,
    .release = cache_release,
    .begin_op = cache_begin_op,
    .sync = cache_sync,
    .end_op = cache_end_op,
    .alloc = cache_alloc,
    .free = cache_free,
};
