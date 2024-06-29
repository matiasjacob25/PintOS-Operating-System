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
  
  // implement some function to verify invalid esp
  // should be able to generalize to any pointers
  int *esp = f->esp; 
  // is_user_addr(esp)

  // extract the syscall number
  unsigned syscall_number = *(esp);

  switch (syscall_number)
  {
    case SYS_HALT:
      shutdown_power_off();
      break;  
    case SYS_EXIT:
      handle_sys_exit();
      break; 
    case SYS_EXEC:
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
      //syscall, fd, buffer, size 
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
        // TODO: need to verify that current thread has already open 
        // the file with descriptor == fd
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

void
handle_sys_exit (void)
{
  thread_exit();
}
