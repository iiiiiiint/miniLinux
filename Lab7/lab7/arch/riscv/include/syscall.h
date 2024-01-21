#ifndef _SYSCALL_H
#define _SYSCALL_H
#include "proc.h"
#include "printk.h"
#include "defs.h"

#define SYS_OPENAT  56
#define SYS_CLOSE   57
#define SYS_LSEEK   62
#define SYS_READ    63
#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_CLONE   220

void syscall(struct pt_regs *regs);
#endif