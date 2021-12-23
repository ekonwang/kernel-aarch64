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

static bool _cache_debug = true; // output info in debug mode.

static SpinLock lock;     // protects block cache.
static LogHeader header;  // in-memory copy of log header block.
static Arena arena;       // memory pool for `Block` struct.
static usize outstanding; // how many syscall is made.
static usize end_pending; // end_op but have not commit yet.
static usize cached_num;  // total cached block number.

static void unsafe_crash_recover(bool _safe);

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
    init_spinlock(&lock, "general lock for block cache");
    _cache_debug = false;
    cached_num = 0;
    end_pending = 0;
    // printf("\nlock addr -> %p\n", &lock);

    // if neccessary, recover from crash.
    unsafe_crash_recover(true);
}

// exile cached block from arena.
void exile_cache(Block *blk) {
    free_object(blk);
    cached_num -= 1;
}

// initialize a block struct.
static void init_block(Block *block) {
    block->block_no = 0;
    block->acquired = false;
    block->pinned = false;
    block->valid = false;
    init_list_node(&block->node);
    init_sleeplock(&block->lock, "block");
    memset(&(block->data), 0, sizeof(block->data));
}

// see `cache.h`.
usize get_num_cached_blocks() {
    return cached_num;
}

/* caller should hold the lock */
static Block *unsafe_cache_acquire(usize block_no, bool _safe) {
    Block *blk = NULL;
    if (_safe) 
        acquire_spinlock(&lock);
    while(1) {
        blk = get_cache(block_no);
        if (blk == NULL) {
            blk = (Block *)alloc_object(&arena);
            init_block(blk);
            insert_cache(blk);
            
            cached_num += 1;
            blk->valid = true;
            blk->block_no = block_no;
            device->read(block_no, &blk->data);
        }
        if (blk != NULL) break;
    }
    acquire_sleeplock(&(blk->lock));
    blk->acquired = true;

    if (_cache_debug) 
        printf("\n \033[46;37;5m cache_acquire \033[0m: now cached blocks: %d\n", get_num_cached_blocks());
    if (get_num_cached_blocks() > EVICTION_THRESHOLD) 
        scavenger();
    if (_safe)
        release_spinlock(&lock);
    return blk;
}

// see `cache.h`.
static Block *cache_acquire(usize block_no) {
    unsafe_cache_acquire(block_no, true);
}

// see `cache.h`.
static void cache_release(Block *block) {
    block->acquired = false;
    release_sleeplock(&(block->lock));
}

// see `cache.h`.
static void cache_begin_op(OpContext *ctx) {
    acquire_spinlock(&lock);
    while ((outstanding + 1) * OP_MAX_NUM_BLOCKS + end_pending > sblock->num_log_blocks) {
        sleep(&lock, &lock);
    }
    outstanding += 1;
    ctx->ops_cnt = 0;
    release_spinlock(&lock);
}

static void unsafe_cache_sync(OpContext *ctx, Block *block, bool _safe) {
    if (_safe)
        acquire_spinlock(&lock);
    if (ctx) {
        usize i;
        for (i = 0; i < header.num_blocks; i++) {
            if (header.block_no[i] == block->block_no) 
                break;
        }
        if (i == header.num_blocks) {
            header.block_no[i] = block->block_no;
            header.num_blocks += 1;
            ctx->ops_cnt += 1;
        }
        block->pinned = true;

        if (ctx->ops_cnt > OP_MAX_NUM_BLOCKS) {
            PANIC("operation numbers exceed limit");
        }

        if (_cache_debug) {
            printf("\n %s: \033[41;37;5mcache_sync\033[0m: new write-into, now pending tasks: %d \n", __FILE__, ctx->ops_cnt);
            printf("\033[41;37;5m cache_sync\033[0m: now overall pending tasks: %d \033[0m \n", header.num_blocks);
        }
    } else
        device_write(block);
    if (_safe)
        release_spinlock(&lock);
}

// see `cache.h`.
static void cache_sync(OpContext *ctx, Block *block) {
    unsafe_cache_sync(ctx, block, true);
}

/* Kind reminder of Yikun: caller must hold lock. */
static void unsafe_crash_recover(bool _safe) {
    if (_safe)
        acquire_spinlock(&lock);
    usize i;
    u8 buffer[BLOCK_SIZE];
    usize start = sblock->log_start; 
    
    read_header();
    if (_cache_debug) {
        printf("\n %s: \033[45;37;5mcache_recover\033[0m: now %d blocks need to be transfered from logging area to data area \n", __FILE__, header.num_blocks);
        printf("\033[45;37;5mcache_recover\033[0m: starting transfer... \n");
    }
    for (i = 0; i < header.num_blocks; i++) {
        device->read(start + 1 + i, &(buffer));
        device->write(header.block_no[i], &(buffer));
    }
    header.num_blocks = (usize)0;
    write_header();

    if (_safe)
        release_spinlock(&lock);
}

static void commit() {
    Block *from;
    usize i;
    usize start = sblock->log_start; 

    if (_cache_debug) {
        printf("\n%s: \033[43;37;5mcache_commit\033[0m: now overall pending tasks: %d \n", __FILE__, header.num_blocks);
        printf("\033[43;37;5mcache_commit\033[0m: starting writing logging area and header...\n");
    }

    for (i = 0; i < header.num_blocks; i++) {
        from = get_cache(header.block_no[i]);
        device->write(start + 1 + i, &(from->data)); 
        // unpin the block cache.
        from -> pinned = false;
    }
    write_header();
    unsafe_crash_recover(false);

    if (_cache_debug) {
        printf("\n%s: \033[44;37;5mcache_commit\033[0m: transfer done. \n", __FILE__);
        printf("\033[44;37;5mcache_commit\033[0m: remaining %d tasks.\n", header.num_blocks);
    }
}

// see `cache.h`.
static void cache_end_op(OpContext *ctx) {
    acquire_spinlock(&lock);
    if (outstanding == 1) {
        commit();
        end_pending = 0;
    } else{
        end_pending += ctx->ops_cnt;
    }
    outstanding -= 1;
    wakeup(&lock);
    release_spinlock(&lock);
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static usize cache_alloc(OpContext *ctx) {
    acquire_spinlock(&lock);
    usize block_no = -1;
    usize blki, blkj;
    usize totblk = sblock->num_blocks;
    usize bitmap_start = sblock->bitmap_start;
    u8 buffer[BLOCK_SIZE];

    for (blki = 0; blki < totblk; blki += BIT_PER_BLOCK) {
        Block *block = unsafe_cache_acquire(bitmap_start + blki/BIT_PER_BLOCK, false);
        for (blkj = 0; blkj < BIT_PER_BLOCK && blki + blkj < totblk; blkj++) {
            if (bitmap_get(&(block->data), blkj) == false) {

                bitmap_set(&(block->data), blkj);
                block_no = blki + blkj;
                unsafe_cache_sync(ctx, block, false);
                cache_release(block);

                memset(&buffer, 0, BLOCK_SIZE);
                device->write(block_no, &buffer);

                release_spinlock(&lock);
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
    acquire_spinlock(&lock);

    usize blki = (block_no) / BIT_PER_BLOCK;
    usize blkj = (block_no) % BIT_PER_BLOCK;
    usize bitmap_start = sblock->bitmap_start;
    Block *block = unsafe_cache_acquire(bitmap_start + blki, false);

    if (bitmap_get(&(block->data), blkj) == false) {
        PANIC("cache_free: trying free a free block");
    }
    bitmap_clear(&(block->data), blkj);
    unsafe_cache_sync(ctx, block, false);
    cache_release(block);
    release_spinlock(&lock);
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
