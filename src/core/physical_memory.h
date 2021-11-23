#pragma once

#ifndef _CORE_MEMORY_MANAGE_
#define _CORE_MEMORY_MANAGE_

#include <common/spinlock.h>
#define PAGEPOOLSIZE 200000

/* typedef struct {
    void *struct_ptr;
    void (*page_init)(void *datastructure_ptr, void *start, void *end);
    void *(*page_alloc)(void *datastructure_ptr);
    void (*page_free)(void *datastructure_ptr, void *page_address);
    SpinLock pmemlock;
} PMemory; */

/* typedef struct {
    void *next;
} FreeListNode; */

typedef struct {
    void (*page_init)(void *start, void *end);
    void *(*page_nalloc)(int numpages);
    void (*page_nfree)(void *page_address, int numpages);
    SpinLock pmemlock;
} PMemory;

void init_memory_manager(void);
/* void free_range(void *start, void *end); */
void *kalloc(void);
void *nkalloc(int numpages);
void kfree(void *page_address);
void nkfree(void *page_address);

#endif
