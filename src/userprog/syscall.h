#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "lib/user/syscall.h"

/* lock for file system operations */
struct lock filesys_lock;

/* Struct that is to be inserted into a thread's 
file descriptor table (fdt). */
struct thread_file
{
  int fd;
  struct file *file_addr;
  struct list_elem file_elem;
};

/* provides information about a memory mapped file */
struct file_mapping {
  // file_mapping id
  mapid_t id;
  // file being mapped
  struct file *file;
  // virtual address (in user address space) that file is being mapped to 
  void *addr;
  // number of pages being mapped
  int page_cnt; 
  struct list_elem file_mapping_elem;
};

void syscall_init (void);
void handle_sys_exit (int error_status);
int handle_sys_read(int fd, char *buf_addr, unsigned size);
int handle_sys_write(int fd, char *buf_addr, unsigned size);
mapid_t handle_sys_mmap(int fd, void *addr_);
void handle_sys_munmap(mapid_t id);
void *validate_addr (void *ptr);
void validate_buffer (void *b);
int fdt_push(struct file *f);
struct file *get_open_file(int fd);
struct file_mapping *get_file_mapping(mapid_t id_);

#endif /* userprog/syscall.h */
