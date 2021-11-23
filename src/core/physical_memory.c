#include <aarch64/mmu.h>
#include <common/types.h>
#include <core/physical_memory.h>
#include <core/console.h>
#include <common/string.h>

extern char end[];
PMemory pmem; /* : Lab4 multicore: Add locks where needed */
/* FreeListNode head; */
static char pagepool[PAGEPOOLSIZE];
static u64 poolnumend = PAGEPOOLSIZE;
static void *pagestart = NULL;
/*
 * Editable, as long as it works as a memory manager.
 */
static size_t phystop = 0x3F000000;
/* static void freelist_init(void *datastructure_ptr, void *start, void *end);
static void *freelist_alloc(void *datastructure_ptr);
static void freelist_free(void *datastructure_ptr, void *page_address); */
static void pool_init(void *start, void *end);
static void *pool_alloc(int numpages);
static void *pool_free(void *page_address, int numpages);


static void pool_init(void *start, void *end) {
    u64 pagenum = ((char *)end - (char *)start) / PAGE_SIZE;
    poolnumend = MIN(poolnumend, pagenum);
    pagestart = start;
    for (int i = 0; i < poolnumend; i++) {
        pagepool[i] = (char) 0;
    }
}

static void *pool_alloc(int numpages) {
    int i, j;
    void *page = NULL;
    acquire_spinlock(&pmem.pmemlock);
    for (i = 0; i <= poolnumend - numpages; i++) {
        for (j = 0; j < numpages; j++) {
            if (pagepool[i+j]) 
                break;
            if (j + 1 == numpages)
                page = pagestart + PAGE_SIZE * i;
        }
        if (page) 
            break;
        i += j;
    }
    if (page) {
        for (j = i; j < i+numpages; j++) {
            pagepool[j] = 0x1;
        }
    }
    release_spinlock(&pmem.pmemlock);
    return page;
}

static void *pool_free(void *page_address, int numpages) {
    if ((u64)page_address % PAGE_SIZE) 
        PANIC("kmem: page address not aligned.\n");
    bool already_alloc = false;
    int localstart = ((u64)page_address - (u64)pagestart) / PAGE_SIZE;
    printf("pagestart:%p currentpage:%p\n", pagestart, page_address);
    assert(localstart + numpages <= poolnumend);
    
    acquire_spinlock(&pmem.pmemlock);
    for (int i = localstart; i < localstart + numpages; i++) {
        if (!pagepool[i]) 
            PANIC("kmem: refree a free physical page.");
        pagepool[i] = (char) 0;
    }
    release_spinlock(&pmem.pmemlock);
}

static void init_PMemory(PMemory *pmem_ptr) {
    pmem_ptr->page_init = pool_init;
    pmem_ptr->page_nalloc = pool_alloc;
    pmem_ptr->page_nfree = pool_free;
}

void init_memory_manager(void) {
    // HACK Raspberry pi 4b.
    // size_t phystop = MIN(0x3F000000, mbox_get_arm_memory());
    
    // notice here for roundup
    void *ROUNDUP_end = ROUNDUP((void *)end, PAGE_SIZE);
    init_PMemory(&pmem);
    init_spinlock(&pmem.pmemlock, "pmem");

    pmem.page_init(ROUNDUP_end, (void *)P2K(phystop));
}

void *nkalloc(int numpages) {
    void *p = pmem.page_nalloc(numpages);
    if (p == NULL) 
        PANIC("kmem: nkalloc fails.");
    return p;
}

void nfree(void *page_address, int numpages) {
    pmem.page_nfree(page_address, numpages);
}

void *kalloc(void) {
    void *p = pmem.page_nalloc(1);
    if (p == NULL)
        PANIC("kmem: kalloc fails.");
    return p;
}

void kfree(void *page_address) {
    pmem.page_nfree(page_address, 1);
}

/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */
/* static void *freelist_alloc(void *datastructure_ptr) {
    FreeListNode *f = (FreeListNode *)datastructure_ptr;
    acquire_spinlock(&pmem.pmemlock);
    if((int64_t)pmem.struct_ptr != NULL){
        f = pmem.struct_ptr;
        pmem.struct_ptr = ((FreeListNode*)pmem.struct_ptr) -> next;
    }
    release_spinlock(&pmem.pmemlock);
    return f;
} */

/*
 * Free the page of physical memory pointed at by page_address.
 */
/* static void freelist_free(void *datastructure_ptr, void *page_address) {
    FreeListNode *f = (FreeListNode *)datastructure_ptr;
    FreeListNode* p = page_address;
    acquire_spinlock(&pmem.pmemlock);
    p -> next = pmem.struct_ptr;
    pmem.struct_ptr = p;
    release_spinlock(&pmem.pmemlock);
} */

/*
 * Record all memory from start to end to freelist as initialization.
 */

/* static void freelist_init(void *datastructure_ptr, void *start, void *end) {
    FreeListNode *f = (FreeListNode *)datastructure_ptr;
    
    // start, end all virtual address.
    acquire_spinlock(&pmem.pmemlock);
    pmem.struct_ptr = NULL;
    release_spinlock(&pmem.pmemlock);
    for(char* p = start; p + PAGE_SIZE <= (char*)end; p += PAGE_SIZE) {
        freelist_free(f, p);
    }
} */

/* static void init_PMemory(PMemory *pmem_ptr) {
    pmem_ptr->page_init = freelist_init;
    pmem_ptr->page_alloc = freelist_alloc;
    pmem_ptr->page_free = freelist_free;
} */

/* void init_memory_manager(void) {
    // HACK Raspberry pi 4b.
    // size_t phystop = MIN(0x3F000000, mbox_get_arm_memory());
    
    // notice here for roundup
    void *ROUNDUP_end = ROUNDUP((void *)end, PAGE_SIZE);
    init_PMemory(&pmem);
    init_spinlock(&pmem.pmemlock, "pmem");

    pmem.page_init(pmem.struct_ptr, ROUNDUP_end, (void *)P2K(phystop));
} */

/*
 * Record all memory from start to end to memory manager.
 */
/* void free_range(void *start, void *end) {
    for (void *p = start; p + PAGE_SIZE <= end; p += PAGE_SIZE)
        pmem.page_free(pmem.struct_ptr, p);
} */

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 * Corrupt the page by filling non-zero value in it for debugging.
 */
/* void *kalloc(void) {
    void *p = pmem.page_alloc(pmem.struct_ptr);
    return p;
} */

/* Free the physical memory pointed at by page_address. */
/* void kfree(void *page_address) {
    pmem.page_free(pmem.struct_ptr, page_address);
} */
