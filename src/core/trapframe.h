#pragma once

#ifndef _CORE_TRAPFRAME_H_
#define _CORE_TRAPFRAME_H_

#include <common/defines.h>

typedef struct {
	/* TODO: Lab3 Interrupt */
    u64 SP_EL0;
	u64 SPSR_EL1;
	u64 ELR_EL1;
	
} Trapframe;

#endif
