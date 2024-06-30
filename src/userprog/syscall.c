#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

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
      f->eax = handle_sys_exec(*(esp+1));
      break; 
    case SYS_WAIT:
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
      int fd = *(esp+1);
      int bytes_written = 0;
      const char *buf_addr = *(esp+2);
      unsigned size = *(esp+3);

      // print to console via putbuf
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
      f->eax = bytes_written;
      break; 
    case SYS_SEEK:
      break; 
    case SYS_TELL:
      break; 
    case SYS_CLOSE:
      break; 
    default:
      printf("invalid system call number\n");
  }

  printf ("system call %u\n", syscall_number);
}


// handler for SYS_EXIT
void 
handle_sys_exit(int exit_code){
  printf ("%s: exit(%d)\n", thread_current()->name, exit_code);
  thread_exit();
}

// handler for SYS_EXEC
pid_t
handle_sys_exec (const char *cmd_line)
{
  return process_execute(cmd_line);
}

// Checks validity of the user-provided address.
// If p is invalid, the process exits (and frees its resources via sys_exit).
void *
validate_addr (void *p){
  // check if p is a null pointer.
  // check if p points to user address.
  // check if p points to unmapped virtual memory.
  if (p == NULL ||
      !is_user_vaddr(p) ||
      !pagedir_get_page(thread_current()->pagedir, p))
  {
    handle_sys_exit();
  }
  return p;
}