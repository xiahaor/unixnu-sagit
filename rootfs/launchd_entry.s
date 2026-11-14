    .text
    .globl _start
_start:
    // Kernel hands pid 1 a stack that is only 8-byte aligned; AArch64 needs 16
    // whenever SP is used as a base register.
    mov  x9, #-16
    mov  x0, sp
    and  x0, x0, x9
    mov  sp, x0
    bl   _main_user
1:  b    1b
