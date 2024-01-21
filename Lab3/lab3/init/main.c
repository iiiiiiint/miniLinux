#include "printk.h"
#include "sbi.h"

extern void test();
extern char _stext[];

int start_kernel() 
{
    printk("2023");
    printk(" Hello RISC-V\n");

    _stext[0] = (uint64)0x1;
    // printk("%lu\n",_stext[0]);
    test(); // DO NOT DELETE !!!

	return 0;
}
