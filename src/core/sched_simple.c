#include <common/spinlock.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

#ifndef MULTI_SCHEDULER
struct {
    struct proc proc[NPROC];
    SpinLock lock;
} ptable /* : Lab4 multicore: Add locks where needed in this file or others */;

/* 
 * Function headers of scheduler APIs.
 * Other file would use wrapper of these services.
 * Such as :
 * alloc_pcb_simple -> alloc_pcb
 * acquire_ptable_lock -> acquire_proc_lock
 * release_ptable_lock -> release_proc_lock
 */
static void scheduler_simple();
static struct proc *alloc_pcb_simple();
static void sched_simple();
static void init_sched_simple();
static void acquire_ptable_lock();
static void release_ptable_lock();
static bool hold_ptable_lock();
struct sched_op simple_op = {.scheduler = scheduler_simple, 
                             .alloc_pcb = alloc_pcb_simple, 
                             .sched = sched_simple,
                             .acquire_ptable_lock = acquire_ptable_lock, 
                             .release_ptable_lock = release_ptable_lock,
                             .hold_ptable_lock = hold_ptable_lock,
                             .init = init_sched_simple
};
struct scheduler simple_scheduler = {.op = &simple_op};

/* 
 * Process identity starts at number 1.
 * Function swtch : simple implementation between process contexts.
 * |                                    ILLUSTRATION                                 |
 * |                                        swtch  <== User program running context. |
 * | Kernel (scheduler) running context <== swtch                                    |
 * | Kernel (scheduler) running context ==> swtch                                    |
 * |                                        swtch  ==> User program running context  |
 * |                                         ...                                     |
 */
int nextpid = 1;
void swtch(struct context **, struct context *);
void to_forkret();

static void init_sched_simple() {
    init_spinlock(&ptable.lock, "ptable");
}

static void acquire_ptable_lock() {
    acquire_spinlock(&ptable.lock);
}

static void release_ptable_lock() {
    release_spinlock(&ptable.lock);
}

static bool hold_ptable_lock() {
    return holding_spinlock(&ptable.lock);
}
/*
 * Per-CPU process scheduler
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns.  It loops, doing:
 *  - choose a process to run
 *  - swtch to start running that process
 *  - eventually that process transfers control
 *        via swtch back to the scheduler.
 */
static void 
scheduler_simple() {
    struct proc *p;
    struct cpu *c = thiscpu();
    c->proc = NULL;
    unsigned int proc_num = 0;

    while(1) {
        /* Loop over process table looking for process to run. */
        /* : Lab3 Schedule */
        acquire_ptable_lock();
        p = (struct proc*)(&ptable) + proc_num;
        if (p -> state == RUNNABLE) {
            uvm_switch(p -> pgdir);
            c->proc = p;
            c->proc->state = RUNNING;
            printf("scheduler: process id (pid:%d) takes the cpu %d\n", p->pid, cpuid());
            swtch(&c->scheduler->context, c->proc->context);
            c->proc = NULL;
        }
        release_ptable_lock();
/*         if (p->state != UNUSED)
            printf("process at slot %d used : %d\n", proc_num, p->state);
 */
        /* proc_num = (proc_num + 1) % NPROC;
        printf("cpuid : %d  =>  proc_id : %d\n", cpuid(), proc_num);
        if (proc_num == 0) PANIC("reached end"); */
    }
}
/*
 * `Swtch` to thiscpu->scheduler.
 */
static void sched_simple() {
    /* : Lab3 Schedule */
    struct cpu *c = thiscpu();
    struct proc *p = c -> proc; 
    if (!hold_ptable_lock) {
        PANIC("process not holding lock");
    }
    /* else {
        printf("sched: process %d holding lock.\n", (p -> pid));
    } */
    if (p -> state == RUNNING) 
        PANIC("cpu process is running.");
    swtch(&p->context, c->scheduler->context);
}
/* 
 * Allocate an unused entry from ptable.
 * Allocate a new pid for it.
 */
static struct proc *alloc_pcb_simple() {
    /* : Lab3 Schedule */
    struct proc *p = NULL, *start = (struct proc*)&ptable;
    acquire_ptable_lock();
    for (p = start; p < start + NPROC; p++) {
        if (p -> state == UNUSED) {
            p -> pid = nextpid++;
            break;
        }
    }
    p -> state = EMBRYO;
    release_ptable_lock();
    return p;
}
#endif
