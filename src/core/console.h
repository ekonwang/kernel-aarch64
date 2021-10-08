#pragma once

#ifndef _CORE_CONSOLE_H_
#define _CORE_CONSOLE_H_

#include <common/spinlock.h>
#include <common/variadic.h>
#include <core/char_device.h>

#define NEWLINE '\n'

typedef struct {
    SpinLock lock;
    CharDevice device;
} ConsoleContext;

void init_console();

void puts(const char *str);
void vprintf(const char *fmt, va_list arg);
void printf(const char *fmt, ...);

NORETURN void _panic(const char *file, usize line, const char *fmt, ...);

#define PANIC(...) _panic(__FILE__, __LINE__, __VA_ARGS__)
#define panic(__ARGS__) _panic(__FILE__, __LINE__, __ARGS__)
#define assert(x) if (!(x)) { printf("Assertion failed: (%s), file %s, line %d.\n", #x, __FILE__, __LINE__); no_return(); }

#define assert(predicate)                                                                          \
    do {                                                                                           \
        if (!(predicate))                                                                          \
            PANIC("assertion failed: \"%s\"", #predicate);                                         \
    } while (false)

#define asserts(predicate, ...)                                                                    \
    do {                                                                                           \
        if (!(predicate))                                                                          \
            PANIC("assertion failed: \"%s\". %s", #predicate, __VA_ARGS__);                          \
    } while (false)

#endif
