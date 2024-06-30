#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void handle_sys_exit (int error_status);
int handle_sys_write(int fd, const char *buf_addr, unsigned size);
void *validate_addr (void *ptr);


#endif /* userprog/syscall.h */
