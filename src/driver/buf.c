#include <driver/buf.h>

void init_sdbuf() {
    init_buf_lock();
    init_list_node(&head);
}

buf *try_fetch_task() {
    acquire_buf_lock();
    buf *thisbuf = NULL;
    // printf("\n[try_fetch_task] &head: %p\n", &head);
    if (head.next != (ListNode *)&head) {
        // printf("\n[try_fetch_task] head.next: %p\n", head.next);
        thisbuf = (buf *)container_of(head.next, buf, listnode);
    }
    release_buf_lock();
    return thisbuf;
}

buf *fetch_task() {
    buf *thisbuf = try_fetch_task();
    if (thisbuf) {
        acquire_buf_lock();
        // printf("\n[fetch] detaching &thisbuf->listnode: %p\n", &thisbuf->listnode);
        detach_from_list(&thisbuf->listnode);
        release_buf_lock();
    }
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