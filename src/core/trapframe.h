#pragma once

#ifndef _CORE_TRAPFRAME_H_
#define _CORE_TRAPFRAME_H_

#include <common/defines.h>

/*
 * Trapframe should contain register x0~x30,
 * elr_el1, spsr_el1 and  sp_el0.
 * Pay attention to the order of these registers
 * in your trapframe.
 */
typedef struct {
	/* : Lab3 Interrupt */
    u64 SP_EL0;  		// stack pointer.
	u64 SPSR_EL1;		// Pstate : CPU state.
	u64 ELR_EL1;		// PC : program counter.
	u64 r0;				// normal registers from here x0 to x30.
	u64 r1;
	u64 r2;
	u64 r3;
	u64 r4;
	u64 r5;
	u64 r6;
	u64 r7;
	u64 r8;
	u64 r9;
	u64 r10;
	u64 r11;
	u64 r12;
	u64 r13;
	u64 r14;
	u64 r15;
	u64 r16;
	u64 r17;
	u64 r18;
	u64 r19;
	u64 r20;
	u64 r21;
	u64 r22;
	u64 r23;
	u64 r24;
	u64 r25;
	u64 r26;
	u64 r27;
	u64 r28;
	u64 r29;
	u64 r30;
} Trapframe;

#endif
