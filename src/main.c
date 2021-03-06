#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/container.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/trap.h>
#include <core/virtual_memory.h>
#include <driver/clock.h>
#include <driver/interrupt.h>
#include <driver/sd.h>

struct cpu cpus[NCPU];

static SpinLock init_lock = {.locked = 0};

void init_system_once() {
    if (!try_acquire_spinlock(&init_lock))
        return;

    // clear BSS section.
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    init_interrupt();
    init_char_device();
    init_console();
    init_sched();

    init_memory_manager();
    init_virtual_memory();

    // vm_test();
    arena_test();
    init_container();
    sd_init();

    release_spinlock(&init_lock);
}

void hello() {
    printf("CPU %d: HELLO!\n", cpuid());
    reset_clock(1000);
    yield();
}

void init_system_per_cpu() {
    init_clock();
    set_clock_handler(hello);
    init_trap();

    /* : Lab3 uncomment to test interrupt */
    // test_kernel_interrupt();
    init_cpu(&root_container->scheduler);
}

NORETURN void main() {
	/* : Lab1 print */
    init_system_once();
    wait_spinlock(&init_lock);

    init_system_per_cpu();

    if (cpuid() == 0) {
        spawn_init_process();
        // add_loop_test(1);
        // container_test_init();
        bound_processor_pid(1, 0);
        sd_init_idle();
        // add_sd_test();
        enter_scheduler();
    } else {
        enter_scheduler();
    }

    no_return();
}


/* cmake .. -DCMAKE_BUILD_TYPE=Debug
 * make qemu
 */