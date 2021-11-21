#include <common/string.h>
#include <core/arena.h>
#include <core/container.h>
#include <core/physical_memory.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

struct container *root_container = 0;
static Arena arena;
bool do_cont_test = false;

extern void add_loop_test(int times);

/* 
 * The entry of a new spawned scheduler.
 * Similar to `forkret`.
 * Maintain thiscpu()->scheduler.
 */
static NO_RETURN void container_entry() {
    thiscpu()->scheduler->op->scheduler(thiscpu()->scheduler);

	/* container_entry should enter scheduler and should not return */
    PANIC("scheduler should not return");
}

/* 
 * Allocate memory for a container.
 * For root container, a container scheduler is enough. 
 * Memory of struct proc is from another ptable, if root is false.
 * Similar to `alloc_proc`.
 * Initialize some pointers.
 */
struct container *alloc_container(bool root) {
	container *cont = alloc_object(&arena);
    memset(cont, 0, sizeof(container));
    cont->scheduler.op = &simple_op;
    cont->scheduler.cont = cont;
    cont->scheduler.pid = 1;
    init_spinlock(&cont->scheduler.ptable.lock, "ptable");
    init_spinlock(&cont->lock, "container");
    if (root)
        return cont;
    cont->p = alloc_pcb();
    cont->p->is_scheduler = true;
    cont->p->cont = cont;
    for (int i = 0; i < NCPU; i++)
        cont->scheduler.context[i]->r30 = (u64)container_entry;
    return cont;
}

/*
 * Initialize the container system.
 * Initialize the memory pool and root scheduler.
 */
void init_container() {
    ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
    init_arena(&arena, sizeof(container), allocator);
    puts("init arena okay.");
    root_container = alloc_container(true);
}

/* 
 * Allocating resource should be recorded by each ancestor of the process.
 * You can add parameters if needed.
 */
void *alloc_resource(struct container *this, struct proc *p, resource_t resource) {
    if (this == NULL || p == NULL) 
        return NULL;
    
    if (resource == PID) 
    {   
        int foundpid = -1;
        acquire_spinlock(&this->lock);
        for (int i = 0; i < NPROC; i++) 
        {
            if (this->pmap[i].valid == false)
            {
                foundpid = this->scheduler.pid++;
                this->pmap[i].valid == true;
                this->pmap[i].pid_local = foundpid;
                this->pmap[i].p = p;
                break;
            }
        }
        if (foundpid < 0)
            PANIC("alloc_resource : could not allocate new pid.");
        release_spinlock(&this->lock);
        alloc_resource(this->parent, p, PID);
        void *addr = &foundpid;
        return addr;
    }
}

/* 
 * Spawn a new process.
 */
struct container *spawn_container(struct container *this, struct sched_op *op) {
    container *cont = alloc_container(false);
    
    return cont;
}

/*
 * Add containers for test
 */
void container_test_init() {
    struct container *c;

    do_cont_test = true;
    add_loop_test(1);
    c = spawn_container(root_container, &simple_op);
    assert(c != NULL);
}
