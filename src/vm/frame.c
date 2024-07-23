#include "vm/frame.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"

/* Clock hand for page eviction (clock) algorithm. */
static int clock_hand = 0;

/* initializes frame table list and lock*/
void
frame_table_init () 
{
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}

// Returns pointer to the frame table entry containing the physical frame that 
// maps to the virtual user page at address addr. If no such frame exists, 
// returns NULL. 
struct frame_table_entry *
get_frame_table_entry(void* addr_) {
  ASSERT(addr_ != NULL);

  struct list_elem *e;
  for (e = list_begin(&frame_table); 
       e != list_end(&frame_table); 
       e = list_next(e))
  {
    struct frame_table_entry *fte = list_entry(e, 
                                               struct frame_table_entry, 
                                               frame_elem);
    if (fte->spe->addr == addr_)
      return fte;
  }
  return NULL;
}

/* Allocates a fte and frame for the page at page_addr and returns the frame 
address, evicting a page if necessary. If successful, returns a pointer to the 
frame_table_entry. Otherwise, returns NULL. */
void *
frame_alloc (void *page_addr) {
  ASSERT(page_addr != NULL);
  lock_acquire(&frame_table_lock);

  // should not allocate a frame for a page that already has a frame
  if (get_frame_table_entry(page_addr) != NULL)
  {
    lock_release(&frame_table_lock);
    return NULL;
  }

  struct frame_table_entry *fte = NULL;
  void *frame = palloc_get_page (PAL_USER);

  // if palloc_get_page fails, then there is no more space in user pool,
  // and we need to evict one of the pages currently occupying a frame
  if (frame == NULL) {
    if ((fte = frame_evict()) == NULL)
    {
      lock_release(&frame_table_lock);
      return NULL;
    }
  }
  else
  {
    fte = malloc (sizeof (struct frame_table_entry));
    list_push_back (&frame_table, &fte->frame_elem);
    fte->frame = frame;
  }
  fte->owner = thread_current();
  fte->spe = get_sup_page_entry(page_addr);

  lock_release(&frame_table_lock);
  return fte;
}

/* Frees fte frame field, removes fte from the frame table, and frees fte. */
void
frame_free (struct frame_table_entry *fte) {
  ASSERT(fte != NULL);
  
  lock_acquire(&frame_table_lock);
  // update clock_hand if fte is before clock_hand in frame_table
  struct list_elem *e = list_begin(&frame_table);
  for (int i = 0; e != list_end(&frame_table); e = list_next(e), i++) 
  {
    if (e == &fte->frame_elem)
    {
      if (i <= clock_hand)
        clock_hand--;
      break;
    }
  }
  list_remove(&fte->frame_elem);
  palloc_free_page(fte->frame);
  free(fte);
  lock_release(&frame_table_lock);
}

/* Returns a pointer to the frame_table_entry containing the frame that was
chosen to be evicted by the clock algorithm. If the clock algorithm fails to
find a page eviction candidate, returns NULL. */

void * 
frame_evict() {
  // page eviction shouldn't be needed if there are no frames in the frame 
  // table to begin with.
  ASSERT(list_size(&frame_table) > 0);

  struct frame_table_entry *fte = NULL;
  struct list_elem *e = list_begin(&frame_table);

  // perform clock algorithm starting at clock_hand.
  for (int i = 0; i < clock_hand; i++)
    e = list_next(e);

  // should NOT need to iterate through frame_table more than once to find
  // a page candidate to evict.
  for (int k = 0;
       k < 2 * list_size(&frame_table);
       clock_hand = (clock_hand + 1) % list_size(&frame_table),
       e = list_next(e),
       k++)
  {
    fte = list_entry(e, struct frame_table_entry, frame_elem);
    if (pagedir_is_accessed(fte->owner->pagedir, fte->spe->addr))
    {
      pagedir_set_accessed(fte->owner->pagedir, fte->spe->addr, false);
    }
    else
    {
      clock_hand = (clock_hand + 1) % list_size(&frame_table);
      frame_page_out(fte->spe->addr);
      return fte;
    } 
  }
  return NULL;
}

/* handler for a frame that has been evicted, or is being manually removed 
from physical memory via call to sup_page_free(). If page is handled 
successfully, returns true. Otherwise, returns false. */
void
frame_page_out(void* page_addr) {
  // int bytes_written = -1;
  struct sup_page_entry *spe = get_sup_page_entry(page_addr);
  
  lock_acquire(&frame_table_lock);
  struct frame_table_entry *fte = get_frame_table_entry(page_addr);
  ASSERT(spe != NULL && fte != NULL);

  // if spe contains file and is dirty, write to back to disk.
  // if spe contains file and is NOT dirty, do nothing.
  if (spe->file != NULL)
  {
    if (pagedir_is_dirty(thread_current()->pagedir, spe->addr))
      file_write_at(spe->file, fte->frame, spe->read_bytes, spe->offset);
  }
  else {
    // if spe contains NO file, then write is physical frame to swap parition.
    swap_to_disk(fte);
  }
  lock_release(&frame_table_lock);

  // if (bytes_written != 0)
  //   return true;
  // return false;
}