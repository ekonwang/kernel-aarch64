#include <core/console.h>
#include <core/proc.h>
#include <core/syscall.h>

void sys_myexecve(char *s) {
    printf("sys_exec: executing %s\n", s);
    return;
}

void sys_myexit() {
    printf("sys_exit: in exit\n");
    exit();
}
