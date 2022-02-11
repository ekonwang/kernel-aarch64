#include <core/syscallno.h>

.global sdtest_start
.global sdtest_end

sdtest_start:
    mov     x8, #SYS_myexit
    svc     #0
    b sdtest_start

.align 4
sdtest_end:
