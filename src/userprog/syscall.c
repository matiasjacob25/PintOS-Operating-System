#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *esp = validate_addr(f->esp);
  unsigned syscall_number = *esp;

  switch (syscall_number)
  {
    case SYS_HALT:
      shutdown_power_off();
      break;  
      
    case SYS_EXIT:
      validate_addr(esp+1);
      handle_sys_exit(*(esp+1));
      break; 

    case SYS_EXEC:
      validate_addr(esp+1);
      f->eax = process_execute(*(esp+1));
      break;

    case SYS_WAIT:
      validate_addr(esp+1);
      process_wait(*(esp+1));
      break; 

    case SYS_CREATE:
      break; 

    case SYS_REMOVE:
      break; 

    case SYS_OPEN:
      break; 

    case SYS_FILESIZE:
      break; 

    case SYS_READ:
      break; 

    case SYS_WRITE: ; 
      validate_addr(esp+1);
      validate_addr(esp+2);
      validate_addr(esp+3);
      f->eax = handle_sys_write(*(esp+1), *(esp+2), *(esp+3));
      break; 

    case SYS_SEEK:
      break; 

    case SYS_TELL:
      break; 

    case SYS_CLOSE:
      break; 

    default:
      NOT_REACHED();
  }
  // testing
  // printf ("system call %u\n", syscall_number);
}

// handler for SYS_EXIT
void 
handle_sys_exit(int exit_status){
  struct thread *cur = thread_current();
  
  // log exit status
  printf ("%s: exit(%d)\n", cur->name, exit_status);

  // update exit status
  cur->exit_status = exit_status;

  // update exit status in parent's child list
  if (cur->parent){
    struct list_elem *e = NULL;
    struct child *c = NULL;
    for (e = list_begin(&cur->parent->children); 
         e != list_end(&cur->parent->children); 
         e = list_next(e))
    {
      c = list_entry(e, struct child, child_elem);
      if (c->pid == cur->tid)
      {
        c->exit_status = exit_status;
        break;
      }
    }
  }

  thread_exit();
}

//handler for SYS_WRITE
int
handle_sys_write(int fd, const char *buf_addr, unsigned size) {
  int bytes_written = 0;

  // print to console via single call to putbuf()
  if (fd == 1)
  {
    putbuf(buf_addr, size);
    bytes_written = size;
  }
  else 
  {
    // TODO: add logic that verifies that current thread has already opened
    // the file with descriptor == fd, before writing to it.
    bytes_written = file_write(fd, buf_addr, size);
  }
  return bytes_written;
}


// Checks validity of the user-provided address.
// If p is invalid, the process exits (and frees its resources via sys_exit).
void *
validate_addr (void *p){
  struct thread *cur = thread_current();
  // check if p is a null pointer.
  // check if p points to user address.
  // check if p points to unmapped virtual memory.
  if (p == NULL ||
      !is_user_vaddr(p) ||
      !pagedir_get_page(cur->pagedir, p))
  {
    handle_sys_exit(cur->exit_status);
  }
  return p;
}