#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <core/virtual_memory.h>
#include <core/physical_memory.h>
#include <common/types.h>
#include <core/console.h>

/* For simplicity, we only support 4k pages in user pgdir. */

extern PTEntries kpgdir;
VMemory vmem;

PTEntriesPtr pgdir_init() {
    return vmem.pgdir_init();
}

PTEntriesPtr pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {
    
    return vmem.pgdir_walk(pgdir, vak, alloc);
}

void vm_free(PTEntriesPtr pgdir) {
    vmem.vm_free(pgdir);
}

int uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
    return vmem.uvm_map(pgdir, va, sz, pa);
}

void uvm_switch(PTEntriesPtr pgdir) {
    // FIXME: Use NG and ASID for efficiency.
    arch_set_ttbr0(K2P(pgdir));
}


/*
 * generate a empty page as page directory
 */

static PTEntriesPtr 
my_pgdir_init() {
    /* TODO: Lab2 memory*/
    PTEntriesPtr pgdir = kalloc();
    memset(pgdir, 0, PAGE_SIZE);
    return pgdir;
}


/*
 * return the address of the pte in user page table
 * pgdir that corresponds to virtual address va.
 * if alloc != 0, create any required page table pages.
 */

static PTEntriesPtr 
my_pgdir_walk(PTEntriesPtr pgdir, void *vak, int alloc) {
    /* TODO: Lab2 memory*/
    int index = 2;
    for(; index > 0; index--) {
        PTEntriesPtr p = &pgdir[PTX(index, (int64_t)vak)];
        if (*p & PTE_VALID) {
            // pte should table rather than block.
            pgdir = (PTEntriesPtr)P2K(PTE_ADDRESS(*p));
        } else{
            if (!alloc || !(pgdir = kalloc())) 
                return 0;
            *p = K2P((int64_t)pgdir) | PTE_TABLE; 
        }
    }
    return &pgdir[PTX(0, (int64_t)vak)];
}

/* A helper function for my_vm_free */
static void 
my_vm_free_helper(PTEntriesPtr pgdir, int64_t index) {
    if(index > 2)
        PANIC('my_vm_free_helper');
    int x = 0;
    PTEntriesPtr p, q;
    for(; x < 512; x++) {
        p = &pgdir[x];
        if (*p & PTE_VALID) {
            q = (PTEntriesPtr)P2K(PTE_ADDRESS(*p));  
            if (index == 0)
                kfree(q);
            else
                my_vm_free_helper(q, index-1);
            }
        }
    }
}

/* Free a user page table and all the physical memory pages. */

void 
my_vm_free(PTEntriesPtr pgdir) {
    /* TODO: Lab2 memory*/
    if((int64_t)pgdir % PAGE_SIZE) 
        PANIC('my_vm_free');
    my_vm_free_helper(pgdir, 2);
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */

int my_uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
    /* TODO: Lab2 memory*/
    char *start = (char *) ROUNDDOWN((uint64_t)va, PAGE_SIZE),
         *end = (char *) ROUNDDOWN((uint64_t)va + sz - 1, PAGE_SIZE),
         *p = start;
    for(; p <= end; p += PAGE_SIZE) {
        PTEntriesPtr pte = my_pgdir_walk(pgdir, va, 1);
        if (pte == NULL) 
            return -1;
        if (*pte & PTE_VALID) {
            PANIC('my_uvm_map : remap');
        }
        *pte =  PTE_ADDRESS(pa) | PTE_VALID;
        pa += PAGE_SIZE;
    }
    return 0;
}

void virtual_memory_init(VMemory *vmem_ptr) {
    vmem_ptr->pgdir_init = my_pgdir_init;
    vmem_ptr->pgdir_walk = my_pgdir_walk;
    vmem_ptr->vm_free = my_vm_free;
    vmem_ptr->uvm_map = my_uvm_map;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void vm_test() {
    /* TODO: Lab2 memory*/
    
    // Certify that your code works!
}