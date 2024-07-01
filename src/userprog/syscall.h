#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void handle_sys_exit (int error_status);
int handle_sys_read(int fd, char *buf_addr, unsigned size);
int handle_sys_write(int fd, char *buf_addr, unsigned size);
void *validate_addr (void *ptr);
int fdt_push(struct file *fdt, struct file *f);
struct file *is_file_open(int fd);

#endif /* userprog/syscall.h */
