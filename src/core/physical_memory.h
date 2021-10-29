#pragma once

#ifndef _CORE_MEMORY_MANAGE_
#define _CORE_MEMORY_MANAGE_

#include <common/spinlock.h>

typedef struct {
    void *struct_ptr;
    void (*page_init)(void *datastructure_ptr, void *start, void *end);
    void *(*page_alloc)(void *datastructure_ptr);
    void (*page_free)(void *datastructure_ptr, void *page_address);
<<<<<<< HEAD
    SpinLock pmemlock;
=======
    SpinLock lock;
>>>>>>> f21804c0df5824df7eb46ef2270cbae703a007f0
} PMemory;

typedef struct {
    void *next;
} FreeListNode;

void init_memory_manager(void);
void free_range(void *start, void *end);
void *kalloc(void);
void kfree(void *page_address);

#endif