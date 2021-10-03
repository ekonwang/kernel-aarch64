#include <aarch64/mmu.h>
#include <core/physical_memory.h>
#include <common/types.h>
#include <core/console.h>

extern char end[];
PMemory pmem;
FreeListNode head;
/*
 * Editable, as long as it works as a memory manager.
 */
static size_t phystop = 0x3F000000;
static void freelist_init(void *datastructure_ptr, void *start, void *end);
static void *freelist_alloc(void *datastructure_ptr);
static void freelist_free(void *datastructure_ptr, void *page_address);


/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */
static void *freelist_alloc(void *datastructure_ptr) {
    FreeListNode *f = (FreeListNode *) datastructure_ptr; 
    /* TODO: Lab2 memory*/
    if((int64_t)f != NULL)
        pmem.struct_ptr = f -> next;
    return f;
}

/*
 * Free the page of physical memory pointed at by page_address.
 */
static void freelist_free(void *datastructure_ptr, void *page_address) {
    FreeListNode* f = (FreeListNode*) datastructure_ptr; 
    /* TODO: Lab2 memory*/
    if ((int64_t)page_address % PAGE_SIZE)
        PANIC('freelist_free : not aligned');
    if (
        // (int64_t)page_address < ROUNDUP((int64_t)end, PAGE_SIZE) || 
        (int64_t)page_address > P2K(phystop)
        )
        PANIC('freelist_free : page_address');
    memset(page_address, 0xf0, PAGE_SIZE);
    FreeListNode* p = page_address;
    p -> next = datastructure_ptr;
    pmem.struct_ptr = p;
}

/*
 * Record all memory from start to end to freelist as initialization.
 */

static void freelist_init(void *datastructure_ptr, void *start, void *end) {
    FreeListNode* f = (FreeListNode*) datastructure_ptr; 
    /* TODO: Lab2 memory*/
    
    // start, end all virtual address.
    if((int64_t)start % PAGE_SIZE || (int64_t)end % PAGE_SIZE)
        PANIC('freelist_init');
    pmem.struct_ptr = NULL;
    for(char* p = start; p <= end; p += PAGE_SIZE) {
        freelist_free(pmem.struct_ptr, p);
    }
}


static void init_PMemory(PMemory *pmem_ptr) {
    pmem_ptr->struct_ptr = (void *)&head;
    pmem_ptr->page_init = freelist_init;
    pmem_ptr->page_alloc = freelist_alloc;
    pmem_ptr->page_free = freelist_free;
}

void init_memory_manager(void) {
    // HACK Raspberry pi 4b.
    // size_t phystop = MIN(0x3F000000, mbox_get_arm_memory());
    
    // notice here for roundup
    void *ROUNDUP_end = ROUNDUP((void *)end, PAGE_SIZE);
    printf("ROUNDUP_end: %llx,P2K(phystop): %llx, %d", 
        (uint64_t)ROUNDUP_end, P2K(phystop), (P2K(phystop) - (uint64_t)ROUNDUP_end)/PAGE_SIZE);
    init_PMemory(&pmem);
    pmem.page_init(pmem.struct_ptr, ROUNDUP_end, (void *)P2K(phystop));
}

/*
 * Record all memory from start to end to memory manager.
 */
void free_range(void *start, void *end) {
    for (void *p = start; p + PAGE_SIZE <= end; p += PAGE_SIZE)
        pmem.page_free(pmem.struct_ptr, p);
}

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 * Corrupt the page by filling non-zero value in it for debugging.
 */
void *kalloc(void) {
    void *p = pmem.page_alloc(pmem.struct_ptr);
    return p;
}

/* Free the physical memory pointed at by page_address. */
void kfree(void *page_address) {
    pmem.page_free(pmem.struct_ptr, page_address);
}