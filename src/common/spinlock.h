#pragma once

#ifndef _COMMON_SPINLOCK_H_
#define _COMMON_SPINLOCK_H_

#include <common/defines.h>
#include <core/cpu.h>

typedef struct {
    char * name;
    volatile bool locked;
    struct cpu *cpu;
} SpinLock;

void init_spinlock(SpinLock *lock, char *);

bool try_acquire_spinlock(SpinLock *lock);
void acquire_spinlock(SpinLock *lock);
void release_spinlock(SpinLock *lock);
void wait_spinlock(SpinLock *lock);
bool holding_spinlock(SpinLock *lock);

#endif