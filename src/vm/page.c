#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include <round.h>

/* initializes the supplementary page table. */
void sup_page_table_init(struct hash *sup_page_table)
{
  hash_init(sup_page_table, 
            sup_page_hash, sup_page_less, 
            NULL);
}

/* Returns a pointer to the sup_page_entry with the page that virtual
address addr exists within. If no such page exists, returns NULL. */
struct sup_page_entry *
get_sup_page_entry(void *addr)
{
  struct sup_page_entry spe;
  struct hash_elem *e;

  spe.addr = (void *)pg_round_down(addr);
  e = hash_find(&thread_current()->sup_page_table, &spe.sup_hash_elem);

  if (e != NULL)
    return hash_entry(e, struct sup_page_entry, sup_hash_elem);
  return NULL;
}

/* returns a hash key that corresponds to page address (excluding offset) 
of h */
unsigned
sup_page_hash(const struct hash_elem *h, void *aux)
{
  const struct sup_page_entry *spe = hash_entry(
      h,
      struct sup_page_entry,
      sup_hash_elem);
  return (((int32_t) spe->addr) >> PGBITS);
}

/* Returns true if page a's address is smaller than page b's. */
bool sup_page_less(const struct hash_elem *a_, const struct hash_elem *b_,
                   void *aux)
{
  const struct sup_page_entry *a = hash_entry(a_, struct sup_page_entry, 
  sup_hash_elem);
  const struct sup_page_entry *b = hash_entry(b_, struct sup_page_entry, 
  sup_hash_elem);
  return a->addr < b->addr;
}

/* References data in supplementary page table entry spe to load data into
a newly allocated page starting at virtual address spe->addr. This function
also adds the newly allocated page into the thread's page table and maps it
to a physical frame. Returns true if the page was successfully loaded and 
mapped. Otherwise, returns false. */
bool 
sup_page_load(struct sup_page_entry *spe)
{ 
  // allocate a physical frame for the page
  struct frame_table_entry *fte = frame_alloc(spe->addr);
  if (fte == NULL)
    return false;
  
  // try to load data from swap partition
  if (spe->swap_idx != -1)
  {
    swap_from_disk(fte);
  }
  // try to load data from disk
  else if (spe->file != NULL)
  {
    // read data from file into frame, and zero out the rest of 
    // the page, if necessary.
    file_seek(spe->file, spe->offset);
    if (file_read (spe->file, fte->frame, spe->read_bytes) != 
        (int) spe->read_bytes)
    {
      palloc_free_page (fte->frame);
      return false;
    }
    memset ((int *) fte->frame + spe->read_bytes, 0, spe->zero_bytes);
  }
  // If not loading from file or swap, must be loading in an all-zeros page
  else
  {
    memset(fte->frame, 0, PGSIZE);
  }

  // update user_page to physical_frame mapping in thread's page table
  if (!install_page (spe->addr, fte->frame, spe->is_writable)) 
  {
    palloc_free_page (fte->frame);
    return false; 
  }
  return true;
}

/* removes and frees sup_page_entry and frame_table_entry corresponding to 
user page page_addr. Also updates thread's page directory to disable mapping
between page_addr and physical memory */
void
sup_page_free(void* page_addr) {
  struct sup_page_entry *spe = get_sup_page_entry(page_addr);
  struct frame_table_entry *fte = NULL;
  // page_addr should exist as a sup_page_entry
  ASSERT(spe != NULL);

  // If page_addr is mapped to physical memory, remove the corresponding
  // frame_table_entry and free the physical frame. 
  lock_acquire(&frame_table_lock);
  fte = get_frame_table_entry(page_addr);
  lock_release(&frame_table_lock);
  if (fte != NULL)
  {
    lock_acquire(&frame_table_lock);
    // 
    frame_page_out(page_addr);
    frame_free(fte);
    lock_release(&frame_table_lock);
  }

  // remove the mapping done by install_page between user address space and 
  // physical frame 
  pagedir_clear_page(thread_current()->pagedir, page_addr);

  // remove page from supplementary page table
  hash_delete(&thread_current()->sup_page_table, &spe->sup_hash_elem);
  free(spe);
}

/* pins the USER page that address addr exists inside. */
void
page_pin(void *addr) {
  if (is_user_vaddr(addr)){
    struct sup_page_entry *spe = get_sup_page_entry(addr);
    spe->is_pinned = true;
  }
}

/* unpins the USER page that address addr exists inside. */
void
page_unpin(void *addr) {
  if (is_user_vaddr(addr)){
    struct sup_page_entry *spe = get_sup_page_entry(addr);
    spe->is_pinned = false;
  }
}