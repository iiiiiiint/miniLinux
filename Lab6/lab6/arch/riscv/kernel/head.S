.section .text.init
.extern start_kernel
.extern sbi_ecall
.extern evaluate_read
.extern evaluate_write
.extern setup_vm
.extern mm_init
.extern task_init
.extern setup_vm_final
.globl _start
_start:
        la t0, boot_stack_top
        li t1, 0xffffffdf80000000
        sub t0, t0, t1
        mv sp, t0

        call setup_vm
        call relocate
        call mm_init
        call setup_vm_final
        call task_init

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

        # csrr t0, sstatus
        # ori t0, t0, 1<<1
        # csrw sstatus, t0

        call start_kernel

.global relocate
relocate:
    li t0, 0xffffffe0002000d0
    csrw stvec, t0

    csrr t0, sie
    ori t0, t0, 1<<5
    csrw sie, t0

    # csrr t0, sstatus
    # ori t0, t0, 1<<1
    # csrw sstatus, t0

    li t0, 0xffffffdf80000000
    add ra, ra, t0
    add sp, sp, t0

    la t0, early_pgtbl
    li t1, 0xffffffdf80000000
    sub t0, t0, t1
    srli t0, t0, 12
    li t1, 8
    slli t1, t1, 60
    or t0, t0, t1
    csrw satp, t0
    
    sfence.vma zero, zero

    fence.i

    ret

.section .bss.stack
.globl boot_stack
boot_stack:
    .space 4096 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top:
