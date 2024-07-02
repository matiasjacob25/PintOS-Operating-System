#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Struct that is to be inserted into a thread's 
file descriptor table (fdt). */
struct thread_file
{
  int fd;
  struct file *file_addr;
  struct list_elem file_elem;
};

// lock for file system operations
struct lock filesys_lock;

void syscall_init (void);
void handle_sys_exit (int error_status);
int handle_sys_read(int fd, char *buf_addr, unsigned size);
int handle_sys_write(int fd, char *buf_addr, unsigned size);
void *validate_addr (void *ptr);
int fdt_push(struct file *f);
struct file *get_open_file(int fd);

#endif /* userprog/syscall.h */