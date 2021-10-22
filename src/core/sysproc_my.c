#include <core/syscall.h>
#include <core/console.h>
#include <core/proc.h>

void sys_myexecve(char *s) {
    printf("sys_exec: executing %s\n", s);
    return;
}

NO_RETURN void sys_myexit() {
    printf("sys_exit: in exit\n");
    exit();
}