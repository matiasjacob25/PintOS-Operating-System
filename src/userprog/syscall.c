#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include <string.h>
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int *esp = validate_addr(f->esp);
  unsigned syscall_number = *esp;
  struct file *file = NULL;
  struct dir *dir = NULL;
  int fd;

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
        // ensure value that file pointer points to is a valid address
        validate_addr(*(esp+1));
        f->eax = process_execute(*(esp+1));
        break;

      case SYS_WAIT:
        validate_addr(esp+1);
        f->eax = process_wait(*(esp+1));
        break; 

      case SYS_CREATE:
        validate_addr(esp+1);
        validate_addr(esp+2);
        // ensure value that file pointer points to is a valid address
        validate_addr(*(esp+1));

        lock_acquire(&filesys_lock);
        f->eax = filesys_create(*(esp+1), *(esp+2), true, false);
        lock_release(&filesys_lock);
        break;

      case SYS_REMOVE:
        validate_addr(esp+1);
        // ensure value that file pointer points to is a valid address
        validate_addr(*(esp+1));

        lock_acquire(&filesys_lock);
        f->eax = filesys_remove(*(esp+1));
        lock_release(&filesys_lock);
        break; 

      case SYS_OPEN:
        validate_addr(esp+1);
        // ensure value that file pointer points to is a valid address
        validate_addr(*(esp+1));

        lock_acquire(&filesys_lock);
        file = filesys_open(*(esp+1));

        if (file == NULL)
          f->eax = -1;
        else
          // add opened file to thread's fdt
          f->eax = fdt_push(file);
        lock_release(&filesys_lock);
        break;

      case SYS_FILESIZE:
        validate_addr(esp+1);
        fd = *(esp+1);
        ASSERT (fd != 0 && fd != 1);

        lock_acquire(&filesys_lock);
        file = get_open_file(*(esp+1));
          
        if (file != NULL)
          {
            f->eax = file_length(file);
            lock_release(&filesys_lock);
          }
        else
          {
            lock_release(&filesys_lock);
            handle_sys_exit(-1);
          }
        break; 

      case SYS_READ:
        validate_addr(esp+1);
        validate_addr(esp+2);
        validate_addr(esp+3);
        // ensure value that buffer pointer points to is a valid address
        validate_addr(*(esp+2));

        lock_acquire(&filesys_lock);
        f->eax = handle_sys_read(*(esp+1), *(esp+2), *(esp+3));
        lock_release(&filesys_lock);
        break; 

      case SYS_WRITE:  
        validate_addr(esp+1);
        validate_addr(esp+2);
        validate_addr(esp+3);
        // ensure value that buffer pointer points to is a valid address
        validate_addr(*(esp+2));

        lock_acquire(&filesys_lock);
        f->eax = handle_sys_write(*(esp+1), *(esp+2), *(esp+3));
        lock_release(&filesys_lock);
        break; 

      case SYS_SEEK:
        validate_addr(esp+1);
        validate_addr(esp+2);

        lock_acquire(&filesys_lock);
        file = get_open_file(*(esp+1));

        if (file != NULL)
          {
            file_seek(file, *(esp+2));
            lock_release(&filesys_lock);
          }
        else
          {
            lock_release(&filesys_lock);
            handle_sys_exit(-1);
          }
        break; 

      case SYS_TELL:
        validate_addr(esp+1);

        lock_acquire(&filesys_lock);
        file = get_open_file(*(esp+1));
        
        if (file != NULL)
          {
            f->eax = file_tell(file);
            lock_release(&filesys_lock);
          }
        else
          {
            lock_release(&filesys_lock);
            handle_sys_exit(-1);
          }
        break; 

      case SYS_CLOSE:
        validate_addr(esp+1);
        fd = *(esp+1);

        lock_acquire(&filesys_lock);
        file = get_open_file(fd);

        if (file != NULL)
          {
            struct thread_file *tf = NULL;
            struct list_elem *e = list_begin(&thread_current()->fdt);

            // if it exists, remove opened file from thread's fdt
            while (e != list_end(&thread_current()->fdt))
              {
                tf = list_entry(e, struct thread_file, file_elem);
                
                if (tf->fd == fd)
                  {
                    file_close(file);
                    list_remove(&tf->file_elem);
                    free(tf);
                    break;
                  }
                e = list_next(e);
              } 
            lock_release(&filesys_lock);
          }
        else
          {
            lock_release(&filesys_lock);
            handle_sys_exit(-1);
          }
        break; 
      
      case SYS_CHDIR:
        validate_addr(esp+1);
        validate_addr(*(esp+1));
        
        dir = dir_open(get_inode_from_path(*(esp+1), false, false, 0));
        if (dir != NULL)
        {
          // close current working directory open new directory.
          dir_close(thread_current()->cwd);
          thread_current()->cwd = dir;
          f->eax = true;
        }
        else
          f->eax = false;
        break;
      
      case SYS_MKDIR:
        validate_addr(esp+1);
        validate_addr(*(esp+1));
        f->eax = filesys_create(*(esp+1), 0, true, true);
        break;

      case SYS_READDIR:
        validate_addr(esp+1);
        validate_addr(esp+2);
        validate_addr(*(esp+2));

        file = get_open_file(*(esp+1));
        if (file != NULL && file->inode->data.is_dir)
        {
          dir = dir_open(file->inode);
          if (dir != NULL)
          {
            f->eax = dir_readdir(dir, *(esp+2));
            break;
          }
        }
        f->eax = false; 
        break;

      case SYS_ISDIR:
        validate_addr(esp+1);

        file = get_open_file(*(esp+1));
        if (file != NULL)
          f->eax = file->inode->data.is_dir;
        else
          handle_sys_exit(-1);
        break;
         
      case SYS_INUMBER:
        validate_addr(esp+1);

        file = get_open_file(*(esp+1));
        if (file != NULL)
          f->eax = file->inode->sector;
        else
          handle_sys_exit(-1);
        break;
      
      default:
        NOT_REACHED();
    }
}

// handler for SYS_EXIT
void 
handle_sys_exit(int exit_status)
{
  struct thread *cur = thread_current();
  
  // log exit status
  printf ("%s: exit(%d)\n", cur->name, exit_status);

  // update exit status
  cur->exit_status = exit_status;

  // update exit status in parent's child list
  if (cur->parent)
    {
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
handle_sys_read(int fd, char *buf_addr, unsigned size)
{
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
      struct file *file = get_open_file(fd);
      if (file != NULL)
        bytes_read = file_read(file, buf_addr, size);
      else
        bytes_read = -1;
    }
  return bytes_read;
}


//handler for SYS_WRITE
int
handle_sys_write(int fd, char *buf_addr, unsigned size)
{
  int bytes_written = 0;

  // print to STDOUT(console) via single call to putbuf()
  if (fd == 1)
    {
      putbuf(buf_addr, size);
      bytes_written = size;
    }
  else 
    {
      struct file *file = get_open_file(fd);
      // only write to file if it is not being executed elsewhere.
      if (file != NULL && file->deny_write == false)
        bytes_written = file_write(file, buf_addr, size);
      else
        bytes_written = 0;
    }
  return bytes_written;
}

// Checks validity of the user-provided address.
// If p is invalid, the process exits (and frees its resources via sys_exit).
void *
validate_addr (void *p)
{
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

// Creates and pushes a thread_file object to the end of the
// running thread's file descriptor table fdt. 
// Returns the file descriptor assigned to the file.
int
fdt_push(struct file *f)
{
  struct thread *cur = thread_current();

  // there can be max 128 newly opened files
  // (in addition to reserved fd 0 and fd 1).
  if (list_size(&cur->fdt) > 128)
    NOT_REACHED(); 

  // create and initialize new thread_file object
  struct thread_file *tf = malloc(sizeof(struct thread_file));
  tf->file_addr = f; 
  tf->fd = cur->next_fd;
  cur->next_fd++;
  list_push_back(&cur->fdt, &tf->file_elem);
   
  return tf->fd;
}

// Returns pointer to file iff the file with descriptor fd exists in the 
// running thread's file descriptor table.
// Otherwise, returns NULL pointer.
struct file *
get_open_file(int fd)
{
  struct thread_file *tf = NULL;
  struct list_elem *e = list_begin(&thread_current()->fdt);
  
  // iterate through elements in the thread's fdt until the file 
  // descriptor is found
  while (e != list_end(&thread_current()->fdt))
    {
      tf = list_entry(e, struct thread_file, file_elem);
      
      if (tf->fd == fd)
        return tf->file_addr;
      e = list_next(e);
    }
  return NULL;
}