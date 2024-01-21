#include "printk.h"
#include "clock.h"
#include "defs.h"

void evaluate_read(){
	uint64 ret = csr_read(sstatus);
	for(int i = 63;i >=0;i--){
	    if((ret&(1UL<<i)) == (1UL<<i)){
	    	printk("1");
	    }else{
	    	printk("0");
	    }
	    if(i%8 == 0){
	    	printk("_");
	    }
	}
	printk("\n");

}
void evaluate_write(){
	uint64 temp = 12138;
	csr_write(sscratch,temp);
	uint64 ret = csr_read(sscratch);
	if(ret == temp){
	    printk("The csr_write is right!\n");
	}else{
	    printk("The csr_write is wrong!\n");
	}

}

void soldier(){
	printk("There!\n");
	return ;
}
