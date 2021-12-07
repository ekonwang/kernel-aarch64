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
        // printf("\n  ¶¶¶ yield_scheduler: this scheduler : %p\n", this);
        thiscpu()->scheduler = this->parent;
        // printf("  ¶¶¶ yield_scheduler: after yield : %p\n", thiscpu()->scheduler);
        yield();
    }
}   

NO_RETURN void scheduler_simple(struct scheduler *this) {
    int has_run;
    while(1){
        for (u64 i = 0; i < NPROC; i++) {
            has_run = 0;
            struct cpu *c = thiscpu();
            acquire_ptable_lock(this);
            proc *p = &this->ptable.proc[i];
            if (p->state == RUNNABLE) {
                // printf("\n  ≤≤≤ [scheduler]: process id (pid:%d)[%p] takes the cpu %d\n", p->pid, p, cpuid());
                has_run = 1;
                uvm_switch(p -> pgdir);
                if (!p->is_scheduler)
                    p->state = RUNNING;
                c->proc = p;

                if (p->is_scheduler) 
                {
                    c->scheduler = &((container *)p->cont)->scheduler;
                    // printf("  ≤≤≤ cpu %d: scheduler CHANGE to : %p\n", cpuid(), c->scheduler);
                    swtch(&this->context[cpuid()], ((container *)p->cont)->scheduler.context[cpuid()]);
                }
                else 
                {
                    // printf("  ≤≤≤ cpu %d: will jump to %p [context : %p]\n", cpuid(), p->context->r30, p->context);
                    swtch(&this->context[cpuid()], p->context);
                }
                yield_scheduler(this);
            }
            release_ptable_lock(this);
        }
    }
}

/* 
NO_RETURN void scheduler_simple(struct scheduler *this) {
    int has_run;
    while(1){
        for (u64 i = 0; i < NPROC; i++) {
            has_run = 0;
            struct cpu *c = thiscpu();
            acquire_ptable_lock(this);
            proc *p = &this->ptable.proc[i];
            if (p->state == RUNNABLE) {
                // printf("\n  ≤≤≤ scheduler: process id (pid:%d)[%p] takes the cpu %d\n", p->pid, p, cpuid());
                has_run = 1;
                uvm_switch(p -> pgdir);
                if (!p->is_scheduler)
                    p->state = RUNNING;
                c->proc = p;
                release_ptable_lock(this);

                if (p->is_scheduler) 
                {
                    c->scheduler = &((container *)p->cont)->scheduler;
                    // printf("  ≤≤≤ cpu %d: scheduler CHANGE to : %p\n", cpuid(), c->scheduler);
                    swtch(&this->context[cpuid()], ((container *)p->cont)->scheduler.context[cpuid()]);
                }
                else 
                {
                    // printf("  ≤≤≤ cpu %d: will jump to %p [context : %p]\n", cpuid(), p->context->r30, p->context);
                    swtch(&this->context[cpuid()], p->context);
                }
            }
            if (has_run)
                yield_scheduler(this);
            else
                release_ptable_lock(this);
        }
    }
} */

static void sched_simple(struct scheduler *this) {
    struct cpu *c = thiscpu();
    proc * p = c->proc;
    // printf("\n  ≥≥≥ [sched]: (cpu %d), process: %p\n", cpuid(), p);
    c->proc = ((container *)this->cont)->p;
    // printf("  ≥≥≥ sched: sched to %p\n", c->proc);
    if (p->is_scheduler) 
        swtch(&((container *)p->cont)->scheduler.context[cpuid()], c->scheduler->context[cpuid()]);
    else
        swtch(&p->context, c->scheduler->context[cpuid()]);
}

static struct proc *alloc_pcb_simple(struct scheduler *this) {
    proc *p = NULL;
    acquire_ptable_lock(this);
    for (int i = 0; i < NPROC; i++) {
        if (this->ptable.proc[i].state == UNUSED) {
            p = &this->ptable.proc[i];
            memset(p, 0, sizeof(proc));
            p->state=EMBRYO;
            p->pid = *(int *)alloc_resource((container *)this->cont, p, PID);
            break;
        }
    }
    release_ptable_lock(this);
    return p;
}

#endif
