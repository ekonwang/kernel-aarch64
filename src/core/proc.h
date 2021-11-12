#pragma once

#include <common/defines.h>
// #include <core/sched.h>
#include <common/spinlock.h>
#include <core/trapframe.h>

#define NPROC      16   /* maximum number of processes */
#define NOFILE     16   /* open files per process */
#define KSTACKSIZE 4096 /* size of per-process kernel stack */

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

/*
 * Context should at least contain callee-saved registers.
 * You can add more information in it.
 */
/* Stack must always be 16 bytes aligned. */
struct context {
    /* : Lab3 Process */
    u64 r15;  // r15 is added into context as an auxiliary register in swtch.
	u64 r16;
	u64 r17;
	u64 r18;
	u64 r19;
	u64 r20;
	u64 r21;
	u64 r22;
	u64 r23;
	u64 r24;
	u64 r25;
	u64 r26;
	u64 r27;
	u64 r28;
	u64 r29;
	u64 r30;
};

struct proc {
    u64 sz;                 /* Size of process memory (bytes)          */
    u64 *pgdir;             /* Page table                              */
    char *kstack;            /* Bottom of kernel stack for this process */
    enum procstate state;    /* Process state                           */
    int pid;                 /* Process ID                              */
    struct proc *parent;     /* Parent process                          */
    Trapframe *tf;           /* Trapframe for current syscall           */
    struct context *context; /* swtch() here to run process             */
    void *chan;              /* If non-zero, sleeping on chan           */
    int killed;              /* If non-zero, have been killed           */
    char name[16];           /* Process name (debugging)                */
    void *cont;
    bool is_scheduler;
	
};
typedef struct proc proc;
void init_proc();
void spawn_init_process();
void yield();
NO_RETURN void exit();
void sleep(void *chan, SpinLock *lock);
void wakeup(void *chan);
