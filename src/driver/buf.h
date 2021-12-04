#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/string.h>

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
    init_buf_lock(&buflock);
}

static void acquire_buf_lock() {
    acquire_spinlock(&buflock);
}

static void release_buf_lock() {
    release_spinlock(&buflock);
}

void init_sdbuf() {
    init_buf_lock();
    init_list_node(&head);
}

// Operation on buflist.

buf *fetch_task() {
    acquire_buf_lock();
    buf *thisbuf = NULL;
    if (head.next != (ListNode *)&head) {
        thisbuf = (buf *)container_of(head.next, buf, listnode);
        detach_from_list(head.next);
    }
    release_buf_lock();
    return thisbuf;
}

void add_task(buf *buf) {
    acquire_buf_lock();
    ListNode *bufhead = &head;
    ListNode *buftail = bufhead->prev;
    buf->listnode.prev = buftail;
    buf->listnode.next = bufhead;
    buftail->next = &(buf->listnode);
    bufhead->prev = &(buf->listnode);
    release_buf_lock();
}
/* 
 * 
 */

/* 
 * Add some useful functions to use your buffer list, such as push, pop and so on.
 */

/* TODO: Lab7 driver. */