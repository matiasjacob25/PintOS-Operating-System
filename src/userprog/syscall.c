#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include <string.h>
#include "lib/user/syscall.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/frame.h"

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
  // store f->esp prior to reference in case of kernel-invoked page fault
  thread_current()->esp = f->esp;

  int *esp = validate_addr(f->esp);
  unsigned syscall_number = *esp;
  struct file *file = NULL;
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
        validate_buffer(*(esp+1));
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
        validate_buffer(*(esp+1));

        lock_acquire(&filesys_lock);
        f->eax = filesys_create(*(esp+1), *(esp+2));
        lock_release(&filesys_lock);
        break;

      case SYS_REMOVE:
        validate_addr(esp+1);
        // ensure value that file pointer points to is a valid address
        validate_buffer(*(esp+1));

        lock_acquire(&filesys_lock);
        f->eax = filesys_remove(*(esp+1));
        lock_release(&filesys_lock);
        break; 

      case SYS_OPEN:
        validate_addr(esp+1);
        // ensure value that file pointer points to is a valid address
        validate_buffer(*(esp+1));

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
        validate_buffer(*(esp+2));
        
        // prevent writes to executable code
        struct sup_page_entry *buf_spe = get_sup_page_entry(*(esp+2));
        if (buf_spe != NULL && buf_spe->is_exec && !buf_spe->is_writable)
          handle_sys_exit(-1);

        // page_pin(*(esp+2));
        lock_acquire(&filesys_lock);
        f->eax = handle_sys_read(*(esp+1), *(esp+2), *(esp+3));
        lock_release(&filesys_lock);
        // page_unpin(*(esp+2));
        break; 

      case SYS_WRITE: ; 
        validate_addr(esp+1);
        validate_addr(esp+2);
        validate_addr(esp+3);
        // ensure value that buffer pointer points to is a valid address
        validate_buffer(*(esp+2));

        // page_pin(*(esp+2));
        lock_acquire(&filesys_lock);
        f->eax = handle_sys_write(*(esp+1), *(esp+2), *(esp+3));
        lock_release(&filesys_lock);
        // page_unpin(*(esp+2));
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

      case SYS_MMAP: 
        validate_addr(esp+1);
        validate_addr(esp+2);
        f->eax = handle_sys_mmap(*(esp+1), *(esp+2));
        break; 

      case SYS_MUNMAP: 
        validate_addr(esp+1);
        handle_sys_munmap(*(esp+1));
        break;

      default:
        // invalid syscall number
        handle_sys_exit(-1);
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


// handler for SYS_WRITE
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

// handler for mmap syscall which creates a single file_mapping and 
// one sup_page_entry per page used to map contents of fd's file to 
// user address space. If successful, returns an id for the mapping. 
// Otherwise, returns -1. 
mapid_t
handle_sys_mmap(int fd, void *addr_)
{
  int read_bytes, zero_bytes_, page_cnt;
  struct sup_page_entry *spe = NULL;
  struct file_mapping *fm = NULL;
  struct file *file = get_open_file(fd);

  // validate fd, addr and file
  if (fd == 0 || 
      fd == 1 || 
      addr_ == NULL || 
      ((int ) addr_ % PGSIZE) != 0 || 
      file == NULL) {
      return -1;
  }

  // should NOT map empty files
  lock_acquire(&filesys_lock);
  if ((read_bytes = file_length(file)) == 0)
  {
    lock_release(&filesys_lock);
    return -1;
  }
  lock_release(&filesys_lock);

  //ensure that the address that file is being mapped to does not overlap with 
  //existing file_mappings
  for (struct list_elem *e = list_begin(&thread_current()->file_mappings);
       e != list_end(&thread_current()->file_mappings);
       e = list_next(e))
  {
    fm = list_entry(e, struct file_mapping, file_mapping_elem);
    if ((fm->addr < addr_) && (addr_ < (fm->addr + fm->page_cnt * PGSIZE)))
      return -1;
  }

  // store the file as a new instance of the file
  if ((fm = malloc(sizeof(struct file_mapping))) == NULL )
    return -1;
  lock_acquire(&filesys_lock);
  fm->file = file_reopen(file);
  lock_release(&filesys_lock);

  // create sup_page_entry for each of the page_cnt pages needed
  // to map fd's file
  zero_bytes_ = PGSIZE - (read_bytes % PGSIZE);
  page_cnt = (read_bytes + zero_bytes_) / PGSIZE;
  int total_read_bytes = 0;
  for (int i = 0; i < page_cnt; i++, total_read_bytes += PGSIZE){
    spe = malloc(sizeof(struct sup_page_entry));
    if (spe != NULL)
    {
      // expect pg_rnd_down(addr_) % addr_ == 0
      spe->addr = (int *) addr_ + i * PGSIZE;
      spe->is_writable = true;
      spe->file = fm->file;
      spe->offset = i * PGSIZE;
      spe->read_bytes = (total_read_bytes + PGSIZE) < read_bytes ? 
                        PGSIZE : PGSIZE - zero_bytes_;
      spe->zero_bytes = spe->read_bytes == PGSIZE ? 0 : zero_bytes_;
      spe->swap_idx = -1;
      spe->is_pinned = false;
      spe->is_exec = false;
    }

    // update thread's hash_table
    if (hash_insert(&thread_current()->sup_page_table, 
      &spe->sup_hash_elem) != NULL)
      return -1;
  }

  // create and store file mapping 
  fm->addr = addr_;
  fm->file = file;
  fm->page_cnt = page_cnt;
  fm->id = thread_current()->next_mapid;
  thread_current()->next_mapid++;
  list_push_back(&thread_current()->file_mappings, &fm->file_mapping_elem);
  return fm->id;
}

void
handle_sys_munmap(mapid_t id)
{
  void *page_addr = NULL;
  struct file_mapping *fm = NULL; 
  if ((fm = get_file_mapping(id)) == NULL)
    NOT_REACHED();
  
  // remove and free sup_page_entry and frame_table_entry data, and remove 
  // page mappings between virtual and physical memory.xw
  struct frame_table_entry *fte = NULL;
  struct sup_page_entry *spe = NULL;
  for (int i = 0; i < fm->page_cnt; i++)
  {
    page_addr = (int *) fm->addr + (i * PGSIZE);
    fte = get_frame_table_entry(page_addr);
    spe = get_sup_page_entry(page_addr);

    // ignore memory-mapped file pages that have already been swapped out
    if (spe->file->inode != fm->file->inode)
      continue;

    // skip writing back memory-mapped pages that have never been loaded.
    if (fte != NULL)
    { 
      lock_acquire(&frame_table_lock);
      // write dirty pages back to disk
      if (pagedir_is_dirty(thread_current()->pagedir, page_addr))
        // file_write_at(spe->file, fte->frame, spe->read_bytes, spe->offset);
        file_write_at(spe->file, fte->frame, spe->read_bytes, spe->offset);
      frame_free(fte);
      lock_release(&frame_table_lock);
    }

    // remove page from supplementary page table
    hash_delete(&thread_current()->sup_page_table, &spe->sup_hash_elem);
    free(spe);
    pagedir_clear_page(thread_current()->pagedir, page_addr);
  }

  // remove file_mapping from file_mappings list
  list_remove(&fm->file_mapping_elem);
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

/* Checks if buffer's contents exists within a page with a valid uaddr,
but has not yet loaded into user memory. If valid, then continue execution. 
Otherwise, terminate the program. */
void
validate_buffer (void *b) 
{
    if (b == NULL || !is_user_vaddr(b))
      handle_sys_exit(thread_current()->exit_status);
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

// Returns a pointer to the file_mapping with id _id, if it exists in 
// current thread's file_mappings list. Otherwise, returns NULL.
struct file_mapping *
get_file_mapping(mapid_t id_)
{
  struct file_mapping *fm = NULL;
  struct list_elem *e = list_begin(&thread_current()->file_mappings);

  // iterate through elements in thread's file_mappings until the 
  // file_mapping with id id is found
  struct thread *cur = thread_current();
  while (e != list_end(&thread_current()->file_mappings))
    {
      fm = list_entry(e, struct file_mapping, file_mapping_elem);
      
      if (fm->id == id_)
        return fm;
      e = list_next(e);
    }
  return NULL;
}