#include <core/console.h>
#include <core/cpu.h>
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

/* myprint(int x) =>
            printf("pid %d, pid in root %d, cnt %d\n", getpid(), getrootpid(), x);
            yield(); */
void sys_myprint(int x) {
    printf("\n\nsys_myprint : enter. current proc = %p\n", thiscpu()->proc);
    int rootpid = -1;
    for (int i = 0; i < NPID; i++) {
        if (root_container->pmap[i].valid && root_container->pmap[i].p == thiscpu()->proc) {
            if (root_container->pmap[i].valid) {
            printf("find valid pid in pmap: i = %d, root_container->pmp[i].p = %p, thiscpu()->p = %p, pid = %d\n"
                , i, root_container->pmap[i].p, thiscpu()->proc, root_container->pmap[i].pid_local);
            }
            rootpid = root_container->pmap[i].pid_local;
        }
    }
    assert(rootpid > 0);
    printf("pid %d, pid in root %d, cnt %d\n", thiscpu()->proc->pid, rootpid, x);
    yield();
}
