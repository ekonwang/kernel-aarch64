#include <core/console.h>
#include <core/syscall.h>
#include <core/syscallno.h>

/*
 * Based on the syscall number, call the corresponding syscall handler.
 * The syscall number and parameters are all stored in the trapframe.
 * See `syscallno.h` for syscall number macros.
 */
void syscall_dispatch(Trapframe *frame) {
    /* : Lab3 Syscall */
    u64 sysnum = frame -> r8;
    switch(sysnum) {
    case SYS_myexecve:
        sys_myexecve((char *)(frame->r0));
        break;
    case SYS_myexit:
        sys_myexit();
        break;
    case SYS_myprint:
        sys_myprint(frame->r0);
        break;
    default:
        PANIC("Unknown syscall number.");
        break;
    }
}
