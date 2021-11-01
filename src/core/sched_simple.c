#include <core/sched.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/virtual_memory.h>

struct {
    struct proc proc[NPROC];
} ptable;

static void scheduler_simple();
static struct proc *alloc_pcb_simple();
static void sched_simple();
struct sched_op simple_op = {
    .scheduler = scheduler_simple, .alloc_pcb = alloc_pcb_simple, .sched = sched_simple};
struct scheduler simple_scheduler = {.op = &simple_op};

int nextpid = 1;
void swtch(struct context **, struct context *);
void to_forkret();
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
        /* TODO: Lab3 Schedule */
        p = (struct proc*)(&ptable) + proc_num;
        if (p -> state == RUNNABLE) {
            uvm_switch(p -> pgdir);
            c->proc = p;
            p->state = RUNNING;
            printf("scheduler: process id %d takes the cpu %d\n", p->pid, cpuid());
            swtch(&c->scheduler->context, p->context);
            c->proc = NULL;
        }
        if (p->state != UNUSED)
            printf("process at slot %d used : %d\n", proc_num, p->state);

        proc_num = (proc_num + 1) % NPROC;
    }
}

/*
 * `Swtch` to thiscpu->scheduler.
 */
static void sched_simple() {
    /* TODO: Lab3 Schedule */
    struct cpu *c = thiscpu();
    struct proc *p = c -> proc;
    if (p -> state == RUNNING) 
        PANIC("cpu process is running.");
    swtch(&p->context, c->scheduler->context);
}

/* 
 * Allocate an unused entry from ptable.
 * Allocate a new pid for it.
 */
static struct proc *alloc_pcb_simple() {
    /* TODO: Lab3 Schedule */
    struct proc *p = NULL, *start = (struct proc*)&ptable;
    for (p = start; p < start + NPROC; p++) {
        if (p -> state == UNUSED) {
            p -> pid = nextpid++;
            break;
        }
    }
    return p;
}
