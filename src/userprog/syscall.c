#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include <string.h>

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
  struct file *file = NULL;
  int fd;
  int fd_idx;

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
      validate_addr(esp+1);
      validate_addr(esp+2);
      f->eax = filesys_create(*(esp+1), *(esp+2));
      break;

    case SYS_REMOVE:
      validate_addr(esp+1);
      f->eax = filesys_remove(*(esp+1));
      break; 

    case SYS_OPEN:
      validate_addr(esp+1);
      // ensure valid pointer is not pointing to NULL
      validate_addr(*(esp+1));
      file = filesys_open(*(esp+1));

      if (file == NULL)
        f->eax = -1;
      else
        // add opened file to thread's fdt
        f->eax = fdt_push(&thread_current()->fdt[0], &file);
      break;

    case SYS_FILESIZE:
      validate_addr(esp+1);
      fd = *(esp+1);
      ASSERT (fd != 0 && fd != 1);
      fd_idx = fd-2;
      file = is_file_open(*(esp+1));
        
      if (file != NULL)
        f->eax = file_length(&thread_current()->fdt[fd_idx]);
      else
        NOT_REACHED();
      break; 

    case SYS_READ:
      validate_addr(esp+1);
      validate_addr(esp+2);
      validate_addr(esp+3);
      f->eax = handle_sys_read(*(esp+1), *(esp+2), *(esp+3));
      break; 

    case SYS_WRITE: ; 
      validate_addr(esp+1);
      validate_addr(esp+2);
      validate_addr(esp+3);
      f->eax = handle_sys_write(*(esp+1), *(esp+2), *(esp+3));
      break; 

    case SYS_SEEK:
      validate_addr(esp+1);
      validate_addr(esp+2);
      file = is_file_open(*(esp+1));

      if (file != NULL)
        file_seek(file, *(esp+2));
      else
        NOT_REACHED();
      break; 

    case SYS_TELL:
      validate_addr(esp+1);
      file = is_file_open(*(esp+1));
      
      if (file != NULL)
        f->eax = file_tell(file);
      else
        NOT_REACHED();
      break; 

    case SYS_CLOSE:
      validate_addr(esp+1);
      fd = *(esp+1);
      fd_idx = fd-2;
      file = is_file_open(fd);

      if (file != NULL)
      {
        file_close(file);
        // remove opened file from thread's fdt
        memset(&thread_current()->fdt[fd_idx], 0, sizeof(struct file));
      }
      else
        NOT_REACHED();
      break; 

    default:
      NOT_REACHED();
  }
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

//handler for SYS_READ
int
handle_sys_read(int fd, char *buf_addr, unsigned size) {
  // assume that you cannot read from STDOUT
  ASSERT(fd != 1);
  int bytes_read = 0, i;

  // read from STDIN(keyboard) using input_getc()
  if (fd == 0)
  {
    for (i = 0; i < size; i++)
    {
      *buf_addr = input_getc();
      buf_addr++;
      bytes_read++;
    }
  }
  else
  {
    struct file *file = is_file_open(fd);
    if (file != NULL)
      bytes_read = file_read(file, buf_addr, size);
    else
      bytes_read = -1;
  }
  return bytes_read;
}


//handler for SYS_WRITE
int
handle_sys_write(int fd, char *buf_addr, unsigned size) {
  // assume that you cannot write to STDIN
  ASSERT(fd != 0);
  int bytes_written = 0;

  // print to STDOUT(console) via single call to putbuf()
  if (fd == 1)
  {
    putbuf(buf_addr, size);
    bytes_written = size;
  }
  else 
  {
    struct file *file = is_file_open(fd);
    if (file != NULL)
      bytes_written = file_write(file, buf_addr, size);
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

// Adds opened file f to first available entry in the 
// file descriptor table fdt.
// Returns the file descriptor assigned to the file.
int
fdt_push(struct file *fdt, struct file *f){
  int i = 0;
  // while(*(&fdt + i*sizeof(struct file)) != NULL)
  //   i++;
  while(fdt[i].inode != NULL)
    i++;
  
  // there can be max 128 newly opened files
  // (in addition to reserved fd 0 and fd 1).
  if (i > 128)
    NOT_REACHED();  
  
  // *(&fdt + i*sizeof(struct file)) = f;
  memcpy(&fdt[i], &f, sizeof(struct file));

  // account for reserved fd 0 and fd 1
  return i+2;
}

// Returns pointer to file iff the file with descriptor fd exists in the 
// running thread's file descriptor table.
// Otherwise, returns NULL pointer.
struct file *
is_file_open(int fd) {
  int fd_idx = fd-2;

  if (fd < 0 || fd_idx > 128 || thread_current()->fdt[fd_idx].inode == NULL)
    return NULL;
  return &thread_current()->fdt[fd_idx];
}