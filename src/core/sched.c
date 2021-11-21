#include <common/defines.h>
#include <core/console.h>
#include <core/container.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

#ifdef MULTI_SCHEDULER

/* Lab6 Add more Scheduler Policies */
static void scheduler_simple(struct scheduler *this);
static struct proc *alloc_pcb_simple(struct scheduler *this);
static void sched_simple(struct scheduler *this);
static void init_sched_simple(struct scheduler *this);
static void acquire_ptable_lock(struct scheduler *this);
static void release_ptable_lock(struct scheduler *this);
struct sched_op simple_op = {.scheduler = scheduler_simple,
                             .alloc_pcb = alloc_pcb_simple,
                             .sched = sched_simple,
                             .init = init_sched_simple,
                             .acquire_lock = acquire_ptable_lock,
                             .release_lock = release_ptable_lock};
struct scheduler simple_scheduler = {.op = &simple_op};

void swtch(struct context **, struct context *);
void to_forkret();

static void init_sched_simple(struct scheduler *this) {
    init_spinlock(&this->ptable.lock, "ptable");
}

static void acquire_ptable_lock(struct scheduler *this) {
    acquire_spinlock(&this->ptable.lock);
}

static void release_ptable_lock(struct scheduler *this) {
    release_spinlock(&this->ptable.lock);
}

/* 
 * Scheduler yields to its parent scheduler.
 * If this == root, just return.
 * Pay attention to thiscpu() structure and locks.
 */
void yield_scheduler(struct scheduler *this) {
    if (this->parent) {
        thiscpu()->scheduler = this->parent;
        yield();
    }
}   

NO_RETURN void scheduler_simple(struct scheduler *this) {
    int has_run;
    while(1){
        has_run = 0;
        acquire_ptable_lock(this);
        for (u64 i = 0; i < NPROC; i++) {
            proc *p = &this->ptable.proc[i];
            struct cpu *c = thiscpu();
            if (p->state == RUNNABLE) {
                has_run = 1;
                uvm_switch(p -> pgdir);
                p->state = RUNNING;
                c->proc = p;
                if (p->is_scheduler) {
                    c->scheduler = p;
                    p->context = ((container *)p->cont)->scheduler.context[cpuid()];
                }
                release_ptable_lock(this);
                swtch(&this->context[cpuid()], p->context);

                break;
            }
        }
        if (!has_run) {
            release_ptable_lock(this);
        }
        yield_scheduler(this);
    }
}

static void sched_simple(struct scheduler *this) {
    struct cpu *c = thiscpu();
    proc * p = c->proc;
    p->state = RUNNABLE;
    c->proc = p->parent;
    swtch(&p->context, c->scheduler->context[cpuid()]);
}

static struct proc *alloc_pcb_simple(struct scheduler *this) {
    proc *p = NULL;
    acquire_ptable_lock(this);
    for (int i = 0; i < NPROC; i++) {
        if (this->ptable.proc[i].state == UNUSED) {
            p = &this->ptable.proc[i];
            p->state=EMBRYO;
            p->pid = *(int *)alloc_resource((container *)this->cont, p, PID);
            break;
        }
    }
    release_ptable_lock(this);
    return p;
}

#endif
