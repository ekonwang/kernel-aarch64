#include <aarch64/intrinsic.h>
#include <common/defines.h>
#include <common/string.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/virtual_memory.h>

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


PTEntriesPtr uvm_copy(PTEntriesPtr pgdir) {
    return vmem.uvm_copy(pgdir);
}

int uvm_alloc(PTEntriesPtr pgdir, usize base, usize stksz, usize oldsz, usize newsz) {
    return vmem.uvm_alloc(pgdir, base, stksz, oldsz, newsz);
}

int uvm_dealloc(PTEntriesPtr pgdir, usize base, usize oldsz, usize newsz) {
    return vmem.uvm_dealloc(pgdir, base, oldsz, newsz);
}

void uvm_switch(PTEntriesPtr pgdir) {
    // FIXME: Use NG and ASID for efficiency.
    arch_set_ttbr0(K2P(pgdir));
}

int copyout(PTEntriesPtr pgdir, void *va, void *p, usize len) {
    return vmem.copyout(pgdir, va, p, len);
}

/*
 * generate a empty page as page directory
 */

static PTEntriesPtr 
my_pgdir_init() {
    /* : Lab2 memory*/
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
    /* : Lab2 memory*/
    int index = 3;
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
    int x = 0;
    PTEntriesPtr p, q;
    for(; x < 512; x++) {
        p = &pgdir[x];
        if (*p & PTE_VALID) {
            q = (PTEntriesPtr)P2K(PTE_ADDRESS(*p));  
            if (index == 0) {
                *p = NULL;
                kfree(q);
            }
            else
                my_vm_free_helper(q, index-1);
        }
    }
}

/* 
 * Fork a process's page table.
 * Copy all user-level memory resource owned by pgdir.
 * Only used in `fork()`.
 */
static PTEntriesPtr my_uvm_copy(PTEntriesPtr pgdir) {
	/* TODO: Lab9 Shell */

    return 0;
}

/* Free a user page table and all the physical memory pages. */

void 
my_vm_free(PTEntriesPtr pgdir) {
    /* : Lab2 memory*/
    my_vm_free_helper(pgdir, 3);
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might not
 * be page-aligned.
 * Return -1 if failed else 0.
 */

int my_uvm_map(PTEntriesPtr pgdir, void *va, size_t sz, uint64_t pa) {
    /* : Lab2 memory*/
    char *start = (char *) ROUNDDOWN((uint64_t)va, PAGE_SIZE),
         *end = (char *) ROUNDDOWN((uint64_t)va + sz - 1, PAGE_SIZE),
         *p = start;
    for(; p <= end; p += PAGE_SIZE) {
        PTEntriesPtr pte = my_pgdir_walk(pgdir, va, 1);
        if (pte == NULL) 
            return -1;
        *pte =  PTE_ADDRESS(pa) | PTE_USER_DATA;
        pa += PAGE_SIZE;
    }
    return 0;
}

/*
 * Allocate page tables and physical memory to grow process
 * from oldsz to newsz, which need not be page aligned.
 * Stack size stksz should be page aligned.
 * Returns new size or 0 on error.
 */

int my_uvm_alloc(PTEntriesPtr pgdir, usize base, usize stksz, usize oldsz, usize newsz) {
    /* TODO: Lab9 Shell */

    return 0;
}

/*
 * Deallocate user pages to bring the process size from oldsz to
 * newsz.  oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsz.  oldsz can be larger than the actual
 * process size.  Returns the new process size.
 */

int my_uvm_dealloc(PTEntriesPtr pgdir, usize base, usize oldsz, usize newsz) {
    /* TODO: Lab9 Shell */

    return 0;
}

/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Allocate physical pages if required.
 * Useful when pgdir is not the current page table.
 * 
 * p is kernel virtual address.
 * va is target user virtual address.
 * 
 * this function works as:
 * memcpy(dest=P2V(walk(pgdir, va, alloc=True)), src=p, size=len)
 */

int my_copyout(PTEntriesPtr pgdir, void *va, void *p, usize len) {
    /* TODO: Lab9 Shell */

    return 0;
}

void virtual_memory_init(VMemory *vmt_ptr) {
    vmt_ptr->pgdir_init = my_pgdir_init;
    vmt_ptr->pgdir_walk = my_pgdir_walk;
    vmt_ptr->uvm_copy = my_uvm_copy;
    vmt_ptr->vm_free = my_vm_free;
    vmt_ptr->uvm_map = my_uvm_map;
    vmt_ptr->uvm_alloc = my_uvm_alloc;
    vmt_ptr->uvm_dealloc = my_uvm_dealloc;
    vmt_ptr->copyout = my_copyout;
}

void init_virtual_memory() {
    virtual_memory_init(&vmem);
}

void test0_yifan_test() {
    *(int64_t *) P2K(0) = 0xac;
    char* p = kalloc();
    printf("kalloc root page table at : %p\n", p);
    memset(p, 0, PAGE_SIZE);
    uvm_map(p, (void *)0x1000, PAGE_SIZE, 0);
    uvm_switch(p);
    PTEntriesPtr pte = pgdir_walk(p, (void *)0x1000, 0);
    if (pte == 0) {
        PANIC(__FILE__, __LINE__, "walk should not return 0\n");
    }
    if (!((uint64_t)pte >> 48)) {
        printf("pte should be virtual address, pte : %p\n", pte);
        PANIC(__FILE__, __LINE__, "");
    } 
    if ((*pte) >> 48) {
        PANIC(__FILE__, __LINE__, "*pte should store physical address.\n");
    }
    printf("pte : %p\n", pte);
    printf("content at 0x1000 : %llx\n", *((int64_t *)0x1000));
    if (*((int64_t *)0x1000) == 0xac) {
        printf("test0 (test_map_region) pass!\n");
    }else {
        printf("test_map_region failed.\n");
    }
}

void test1_pm_kfree_kalloc() {
    // basic test of physical memory API.
    const int test1_len = 10;
    PTEntriesPtr contain[test1_len], contain2[test1_len]; 
    int i = 0;
    for(; i < test1_len; i++)
        contain[i] = kalloc();
    for(; i > 0; i--)
        kfree(contain[i-1]);
    for(; i < test1_len; i++){
        contain2[i] = kalloc();
        assert(contain2[i] == contain[i]);
    }
    for(; i > 0; i--)
        kfree(contain2[i-1]);
    printf("test1_pm pass.\n");
}

void test2_vm_map_walk() {
    // basic test of virtual memory API.
    PTEntriesPtr pgdir = my_pgdir_init();
    PTEntriesPtr v[] = {(PTEntriesPtr)P2K(0x12000)};
    PTEntriesPtr p[] = {0x12000};
    my_uvm_map(pgdir, v[0], 1, (int64_t)p[0]);
    uint64_t a = PTE_ADDRESS(*my_pgdir_walk(pgdir, v[0], 1));
    assert((uint64_t)p[0] == a);
    my_vm_free(pgdir);
    uint64_t b = PTE_ADDRESS(*my_pgdir_walk(pgdir, v[0], 1));
    assert(b == NULL);
    printf("test2_vm pass.\n");
}

void vm_test() {
    /* : Lab2 memory*/
    test0_yifan_test();
    test1_pm_kfree_kalloc();
    test2_vm_map_walk();
    // Certify that your code works!
}
