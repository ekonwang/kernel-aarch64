#include <core/console.h>
#include <core/syscall.h>
#include <core/syscallno.h>

/*
 * Based on the syscall number, call the corresponding syscall handler.
 * The syscall number and parameters are all stored in the trapframe.
 * See `syscallno.h` for syscall number macros.
 */
u64 syscall_dispatch(Trapframe *frame) {
    /* TODO: Lab3 Syscall */
    u64 sysnum = frame -> r8, retval = 0;
    switch(sysnum) {
    case SYS_myexecve:
        sys_myexecve((char *)(frame->r0));
        break;
    case SYS_myexit:
        sys_myexit();
        break;
    default:
        PANIC("Unknown syscall number.");
        break;
    }
	return retval;
}
