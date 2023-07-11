#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "process.h"
//void syscall_init (void);
//defining our file_desc structure
struct file_desc
 {
          struct file *file;
          struct list_elem elem;
          int fd;
 };
void syscall_init(void);
#endif /* userprog/syscall.h */
