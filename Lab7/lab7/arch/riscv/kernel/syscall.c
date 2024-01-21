#include "syscall.h"
#include "fs.h"
#include "defs.h"
#include "types.h"
#include "virtio.h"
#include "fat32.h"

extern struct task_struct* current;

int64_t sys_read(unsigned int fd, char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        ret = target_file->read(target_file, buf, count);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}
int64_t sys_write(unsigned int fd, const char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        ret = target_file->write(target_file, buf, count);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}
int64_t sys_openat(int dfd, const char* filename, int flags) {
    int fd = -1;
    // printk("in sys_openat\n");
    // Find an available file descriptor first
    for (int i = 0; i < PGSIZE / sizeof(struct file); i++) {
        if (!current->files[i].opened) {
            fd = i;
            break;
        }
    }

    // Do actual open
    file_open(&(current->files[fd]), filename, flags);

    return fd;
}
int64_t sys_lseek(int fd, int64_t offset, int whence) {
    // printk("in sys_lseek\n");
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        ret = target_file->lseek(target_file, offset, whence);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}
void sys_close(int fd){
    struct file * target_file = &(current->files[fd]);
    if(target_file->opened){
        target_file->opened = 0;
        target_file->cfo = 0;
    }else{
        printk("file not open\n");
    }
}
void syscall(struct pt_regs *regs){
    regs->sepc += 4;
    if (regs->reg[17] == SYS_WRITE){
        regs->reg[10] = sys_write(regs->reg[10], (const char *)regs->reg[11], regs->reg[12]);
    }else if(regs->reg[17] == SYS_GETPID){
        regs->reg[10] = current->pid;
    }else if(regs->reg[17] == SYS_READ){
        regs->reg[10] = sys_read(regs->reg[10], regs->reg[11], regs->reg[12]);
    }else if(regs->reg[17] == SYS_OPENAT){
        regs->reg[10] = sys_openat(regs->reg[10], regs->reg[11], regs->reg[12]);
    }else if(regs->reg[17] == SYS_LSEEK){
        regs->reg[10] = sys_lseek(regs->reg[10], regs->reg[11], regs->reg[12]);
    }else if(regs->reg[17] == SYS_CLOSE){
        sys_close(regs->reg[10]);
    }
}