#include <core/syscallno.h>

.global sdloop_start
.global sdloop_end

sdloop_start:
    b sdloop_start

.align 4
sdloop_end:
