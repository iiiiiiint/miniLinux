.extern start_kernel
.extern sbi_ecall
.extern evaluate_read
.extern evaluate_write
.extern mm_init
.extern task_init
    .section .text.init
    .globl _start
_start:
        la sp, boot_stack_top
        call mm_init
        call task_init
	   # call evaluate_write
        la t0, _traps
        csrw stvec, t0

    	csrr t0, sie
    	ori t0, t0, 1<<5
    	csrw sie, t0

        sub a0, a0, a0
        sub a1, a1, a1
        rdtime a2
        li t0, 10000000
        add a2, a2, t0
        sub a3, a3, a3
        sub a4, a4, a4
	    sub a5, a5, a5
        sub a6, a6, a6
        sub a7, a7, a7
        call sbi_ecall

        csrr t0, sstatus
        ori t0, t0, 1<<1
        csrw sstatus, t0
        # call evaluate_read

    call start_kernel

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top:
