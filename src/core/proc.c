#include <core/proc.h>
#include <aarch64/mmu.h>
#include <common/string.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>
#include <core/container.h>

extern void to_forkret();
extern void to_sdtest();
extern void trap_return();
/*
 * Look through the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state (allocate stack, clear trapframe, set context for switch...)
 * required to run in the kernel. Otherwise return 0.
 * Step 1 (): Call `alloc_pcb()` to get a pcb.
 * Step 2 (): Set the state to `EMBRYO`.
 * Step 3 (): Allocate memory for the kernel stack of the process.
 * Step 4 (): Reserve regions for trapframe and context in the kernel stack.
 * Step 5 (): Set p->tf and p->context to the start of these regions.
 * Step 6 (): Clear trapframe.
 * Step 7 (): Set the context to work with `swtch()`, `forkret()` and `trap_return()`.
 */
static struct proc *alloc_proc() {
    struct proc *p;
    p = alloc_pcb();
    char* stack = kalloc();

    acquire_sched_lock();
    p -> kstack = stack;
    stack += KSTACKSIZE;
    stack -= sizeof(*(p->tf));
    memset(stack, 0, sizeof(*(p->tf)));
    p -> tf = (Trapframe *)stack;
    stack -= sizeof(*(p->context));
    memset(stack, 0, sizeof(*(p->context)));
    p -> context = (struct context *)stack;
    release_sched_lock();

    return p;
}

/*
 * Set up first user process(Only used once).
 * Step 1: Allocate a configured proc struct by `alloc_proc()`.
 * Step 2 (): Allocate memory for storing the code of init process.
 * Step 3 (): Copy the code (ranging icode to eicode) to memory.
 * Step 4 (): Map any va to this page.
 * Step 5 (): Set the address after eret to this va.
 * Step 6 (): Set proc->sz.
 */
void spawn_init_process() {
    struct proc *p;
    extern char icode[], eicode[];
    u64 cpsize = (u64)(eicode - icode), tmpsize;
    PTEntriesPtr PagePtr;
    p = alloc_proc();
    
    acquire_sched_lock();
    if (p == NULL) 
        PANIC("Could not allocate init process");
    if ((p->pgdir = pgdir_init()) == NULL)
        PANIC("Could not initialize root pagetable");
    printf("Root page table of process p : %p\n", p->pgdir);
    for(u64 vplace = 0; vplace < cpsize; vplace += PAGE_SIZE) {
        PagePtr = kalloc();
        if (PagePtr == NULL) 
            PANIC("kalloc failed");
        tmpsize = (cpsize-vplace > PAGE_SIZE)? PAGE_SIZE : (cpsize-vplace);
        uvm_map(p->pgdir, vplace, tmpsize, K2P(PagePtr));
        memcpy(PagePtr, icode + vplace, tmpsize);
    }
    
    p -> state = RUNNABLE;
    p -> sz = PAGE_SIZE;
    p -> context -> r30 = (u64)to_forkret;

    release_sched_lock();
}

/*
 * A fork child will first swtch here, and then "return" to user space.
 */
void forkret() {
	/* : Lab3 Process */
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
void exit() {
    /* : Lab3 Process */
    // acquire_sched_lock();
    proc * p = thiscpu() -> proc;
    p -> state = ZOMBIE;
    // release_sched_lock();
    printf("process %p at exit.\n", p);
    sched();
    PANIC("ERROR: ZOMBIE trying exit.");
}

/*
 * Give up CPU.
 * Switch to the scheduler of this proc.
 */
void yield() {
    struct cpu *c = thiscpu();
    proc *p = c->proc;
    p->state = RUNNABLE;
    sched();
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void sleep(void *chan, SpinLock *lock) {
    proc *p = thiscpu() -> proc;
    p -> state = SLEEPING;
    printf("\n[sleep] process(pid = %d)[%p]\n", p->pid, p);
    if (lock) {
        acquire_spinlock(lock);
    }
    sched();
    printf("\n[wakeup] process(pid = %d)[%p] wake up.\n", p->pid, p);
}

static void rec_wakeup(void *chan, struct scheduler *this) {
    for (int i = 0; i < NPROC; i++) 
    {
        proc *p = &this->ptable.proc[i];
        acquire_spinlock(&this->ptable.lock);
        if (p->state == SLEEPING && p->chan == chan) 
        {
            p->state = RUNNABLE;
        }
        release_spinlock(&this->ptable.lock);
        if (p->is_scheduler) 
        {
            rec_wakeup(chan, &((container *)(p->cont))->scheduler);
        }
    }
}

/* Wake up all processes sleeping on chan. */
void wakeup(void *chan) {
    struct cpu *c = thiscpu();
    rec_wakeup(chan, &root_container->scheduler);
}

/* 
 * Add process at thiscpu()->container,
 * execute code in src/user/loop.S
 */
void add_loop_test(int times) {
    for (int i = 0; i < times; i++) {
        struct proc *p;
        extern char loop_start[], loop_end[];
        u64 cpsize = loop_end - loop_start, tmpsize;
        PTEntriesPtr PagePtr;
        p = alloc_proc();

        acquire_sched_lock();
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
            memcpy(PagePtr, loop_start + vplace, tmpsize);
        }

        p -> state = RUNNABLE;
        p -> sz = PAGE_SIZE;
        p -> context -> r30 = (u64)to_forkret;

        release_sched_lock();
    }
}

/* Initialize new user program to test SD driver */
void add_sd_test() {
    struct proc *p;
    extern char sdtest_start[], sdtest_end[];
    u64 cpsize = (u64)(sdtest_end - sdtest_start), tmpsize;
    PTEntriesPtr PagePtr;
    p = alloc_proc();
    
    acquire_sched_lock();
    if (p == NULL) 
        PANIC("Could not allocate init process");
    if ((p->pgdir = pgdir_init()) == NULL)
        PANIC("Could not initialize root pagetable");
    printf("Root page table of process p : %p\n", p->pgdir);
    for(u64 vplace = 0; vplace < cpsize; vplace += PAGE_SIZE) {
        PagePtr = kalloc();
        if (PagePtr == NULL) 
            PANIC("kalloc failed");
        tmpsize = (cpsize-vplace > PAGE_SIZE)? PAGE_SIZE : (cpsize-vplace);
        uvm_map(p->pgdir, vplace, tmpsize, K2P(PagePtr));
        memcpy(PagePtr, sdtest_start + vplace, tmpsize);
    }
    
    p -> state = RUNNABLE;
    p -> sz = PAGE_SIZE;
    p -> context -> r30 = (u64)to_sdtest;

    release_sched_lock();
}

/* Initialize new user program to test SD driver */
void add_sd_loop() {
    struct proc *p;
    extern char sdloop_start[], sdloop_end[];
    u64 cpsize = (u64)(sdloop_end - sdloop_start), tmpsize;
    PTEntriesPtr PagePtr;
    p = alloc_proc();
    
    acquire_sched_lock();
    if (p == NULL) 
        PANIC("Could not allocate init process");
    if ((p->pgdir = pgdir_init()) == NULL)
        PANIC("Could not initialize root pagetable");
    printf("Root page table of process p : %p\n", p->pgdir);
    for(u64 vplace = 0; vplace < cpsize; vplace += PAGE_SIZE) {
        PagePtr = kalloc();
        if (PagePtr == NULL) 
            PANIC("kalloc failed");
        tmpsize = (cpsize-vplace > PAGE_SIZE)? PAGE_SIZE : (cpsize-vplace);
        uvm_map(p->pgdir, vplace, tmpsize, K2P(PagePtr));
        memcpy(PagePtr, sdloop_start + vplace, tmpsize);
    }
    
    p -> state = RUNNABLE;
    p -> sz = PAGE_SIZE;
    p -> context -> r30 = (u64)to_forkret;

    release_sched_lock();
}