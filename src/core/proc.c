#include <core/proc.h>
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

void forkret();
extern void trap_return();
/*
 * Look through the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state (allocate stack, clear trapframe, set context for switch...)
 * required to run in the kernel. Otherwise return 0.
 * Step 1 (TODO): Call `alloc_pcb()` to get a pcb.
 * Step 2 (TODO): Set the state to `EMBRYO`.
 * Step 3 (TODO): Allocate memory for the kernel stack of the process.
 * Step 4 (TODO): Reserve regions for trapframe and context in the kernel stack.
 * Step 5 (TODO): Set p->tf and p->context to the start of these regions.
 * Step 6 (TODO): Clear trapframe.
 * Step 7 (TODO): Set the context to work with `swtch()`, `forkret()` and `trap_return()`.
 */
static struct proc *alloc_proc() {
    struct proc *p;
    /* TODO: Lab3 Process */
    // Function alloc_pcb set PCB state from unused to EMBRO.
    p = alloc_pcb();
    char* stack = kalloc();
    if(stack == NULL) 
        PANIC("Could not kalloc.\n");
    
    acquire_proc_lock();
    p -> kstack = stack;
    
    // init sp and trapframe.
    stack += KSTACKSIZE;
    stack -= sizeof(*(p->tf));
    memset(stack, 0, sizeof(*(p->tf)));
    p -> tf = (Trapframe *)stack;

    stack -= sizeof(*(p->context));
    memset(stack, 0, sizeof(*(p->context)));
    p -> context = (struct context *)stack;
    p -> context -> r15 = (u64) forkret;
    p -> context -> r30 = (u64) trap_return;
    release_proc_lock();

    return p;
}

/*
 * Set up first user process(Only used once).
 * Step 1: Allocate a configured proc struct by `alloc_proc()`.
 * Step 2 (TODO): Allocate memory for storing the code of init process.
 * Step 3 (TODO): Copy the code (ranging icode to eicode) to memory.
 * Step 4 (TODO): Map any va to this page.
 * Step 5 (TODO): Set the address after eret to this va.
 * Step 6 (TODO): Set proc->sz.
 */
void spawn_init_process() {
    struct proc *p;
    extern char icode[], eicode[];
    u64 cpsize = (u64)(eicode - icode), tmpsize;
    PTEntriesPtr PagePtr;
    p = alloc_proc();
    
    acquire_proc_lock();
    if (p == NULL) 
        PANIC("Could not allocate init process");
    if ((p->pgdir = pgdir_init()) == NULL)
        PANIC("Could not initialize root pagetable");
    for(u64 vplace = 0; vplace < cpsize; vplace += PAGE_SIZE) {
        PagePtr = kalloc();
        if (PagePtr == NULL) 
            PANIC("kalloc failed");
        tmpsize = (cpsize-vplace > PAGE_SIZE)? PAGE_SIZE : (cpsize-vplace);
        uvm_map(p->pgdir, vplace, tmpsize, K2P(PagePtr));
        memcpy(PagePtr, icode + vplace, tmpsize);
    }
    uvm_switch(p->pgdir);
    p -> state = RUNNABLE;
    p -> sz = ROUNDUP(cpsize, PAGE_SIZE);
    release_proc_lock();
}

/*
 * A fork child will first swtch here, and then "return" to user space.
 */
void forkret() {
	/* TODO: Lab3 Process */
    release_proc_lock();
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
void exit() {
    /* TODO: Lab3 Process */
    acquire_proc_lock();
    proc * p = thiscpu() -> proc;
    p -> state = ZOMBIE;
    release_proc_lock();
    sched();
    PANIC("ZOMBIE trying exit.");
}
