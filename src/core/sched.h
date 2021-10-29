#pragma once

#include <aarch64/intrinsic.h>
#include <common/defines.h>
#include <core/console.h>

struct scheduler;

// enum sched_obj_type {
//     PROCESS,
//     SCHEDULER
// };

// struct sched_obj {
//     enum sched_obj_type type;
// };

struct sched_op {
    void (*init)();
    void (*scheduler)();
    struct proc *(*alloc_pcb)();
    void (*sched)();
    void (*acquire_ptable_lock)();
    void (*release_ptable_lock)();
};

struct scheduler {
    // struct sched_obj sched;
    struct sched_op *op;
    struct context *context;
};

extern struct scheduler simple_scheduler;

struct cpu {
    struct scheduler *scheduler;
    struct proc *proc;
};
#define NCPU 4 /* maximum number of CPUs */
extern struct cpu cpus[NCPU];

static inline struct cpu *
thiscpu()
{
    return &cpus[cpuid()];
}

static inline void init_sched() {
    // thiscpu()->scheduler->op->init();
    simple_scheduler.op->init();
}

static inline void init_cpu(struct scheduler *scheduler) {
    thiscpu()->scheduler = scheduler;
    // init_sched();
}

static inline void enter_scheduler() {
    assert(thiscpu()->scheduler != NULL);
    thiscpu()->scheduler->op->scheduler();
}

static inline void sched() {
    thiscpu()->scheduler->op->sched();
}

static inline struct proc 
*alloc_pcb() {
    assert(thiscpu()->scheduler != NULL);
    assert(thiscpu()->scheduler->op != NULL);
    assert(thiscpu()->scheduler->op->alloc_pcb != NULL);
    return thiscpu()->scheduler->op->alloc_pcb();
}
/* 
 * Lock acquiring API for external file such as proc.c .
 */
static inline void
release_proc_lock() {
    assert(thiscpu()->scheduler != NULL);
	assert(thiscpu()->scheduler->op != NULL);
    assert(thiscpu()->scheduler->op->release_ptable_lock != NULL);
    thiscpu()->scheduler->op->release_ptable_lock();
}
/* 
 * Lock releasing API for external file such as proc.c .
 */
static inline void
acquire_proc_lock() {
    assert(thiscpu()->scheduler != NULL);
	assert(thiscpu()->scheduler->op != NULL);
    assert(thiscpu()->scheduler->op->acquire_ptable_lock != NULL);
    thiscpu()->scheduler->op->acquire_ptable_lock();
}
