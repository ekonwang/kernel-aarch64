#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/string.h>
#include <common/spinlock.h>

#define BSIZE 512

#define B_VALID 0x2 /* Buffer has been read from disk. */
#define B_DIRTY 0x4 /* Buffer needs to be written to disk. */

typedef struct buf {
    int flags;
    u32 blockno;
    ListNode listnode;
    u8 data[BSIZE];  // 1B*512
} buf;

static SpinLock buflock; // Do not forget to initialize it.
static ListNode head; // Do not forget to initialize it.

/*
 * Lock operation. 
 */

static void init_buf_lock() {
    init_spinlock(&buflock, "buflock");
}

static void acquire_buf_lock() {
    acquire_spinlock(&buflock);
}

static void release_buf_lock() {
    release_spinlock(&buflock);
}

void init_sdbuf();

// Operation on buflist.

buf *try_fetch_task();
buf *fetch_task();
void add_task(buf *buf);

/* 
 * Add some useful functions to use your buffer list, such as push, pop and so on.
 */
