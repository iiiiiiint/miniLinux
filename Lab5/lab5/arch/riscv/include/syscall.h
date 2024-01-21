#ifndef _SYSCALL_H
#define _SYSCALL_H
#include "proc.h"
#include "printk.h"
#include "defs.h"

#define SYS_WRITE   64
#define SYS_GETPID  172

void syscall(struct pt_regs *regs);
#endif