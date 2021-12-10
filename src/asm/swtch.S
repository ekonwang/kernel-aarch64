/*
 * Context switch
 *
 *   void swtch(struct context **old, struct context *new);
 *
 * Save the current registers on the stack, creating
 * a struct context, and save its address in *old.
 * Switch stacks to new and pop previously-saved registers.
 *
 * Function `swtch` is call-return semantic.
 * However, the pc+4 of the caller is saved in the context.
 * Therefore, we can store pc+4 in a caller-saved register (like x15),
 * and use `br x15` instead of `ret` in the return stage.
 */
.global swtch
swtch:
    /* : Lab3 Schedule */

    # save callee saved registers
    stp x29, x30, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x17, x18, [sp, #-16]!
    stp x15, x16, [sp, #-16]!

    # swtich sp 
    mov x14, sp
    str x14, [x0]
    mov sp, x1

    # switch to new context
    ldp x15, x16, [sp], #16
    ldp x17, x18, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    ret
