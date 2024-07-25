#include "vm/frame.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/loader.h"

/* Clock hand for page eviction (clock) algorithm. */
static int clock_hand = 0;

/* number of frames in the frame table */
int frame_table_size = 0;

/* initializes frame table list and lock */
void
frame_table_init()
{
  // should use kernel pool, NOT user pool
  frame_table = malloc(sizeof(struct frame_table_entry *) * init_ram_pages);

  // fill frame table with empty frame pages
  void* frame = NULL;
  while((frame = palloc_get_page(PAL_USER)) != NULL)
  { 
    struct frame_table_entry *fte = &frame_table[frame_table_size];
    fte->frame = frame;
    fte->is_mapped = false;
    fte->spe = NULL;
    fte->owner = NULL;
    frame_table_size++;
  }
  lock_init(&frame_table_lock);
  frame_table_size = 12;
}

// Returns pointer to the frame table entry containing the physical frame that
// maps to the virtual user page at address addr (within current thread's
// address space). If no such frame exists, returns NULL.
struct frame_table_entry *
get_frame_table_entry(void *addr_)
{
  ASSERT(lock_held_by_current_thread(&frame_table_lock));
  ASSERT(addr_ != NULL);

  for (int i = 0; i < frame_table_size; i++){
    struct frame_table_entry *fte = &frame_table[i];
    if (fte->is_mapped && fte->spe->addr == addr_ && 
        fte->owner == thread_current())
      return fte;
  }
  // struct list_elem *e;
  // for (e = list_begin(&frame_table);
  //      e != list_end(&frame_table);
  //      e = list_next(e))
  // {
  //   struct frame_table_entry *fte = list_entry(e,
  //                                              struct frame_table_entry,
  //                                              frame_elem);
  //   if (fte->spe->addr == addr_ && fte->owner->tid == thread_current()->tid)
  //     return fte;
  // }
  return NULL;
}

/* Allocates a fte and frame for the page at page_addr and returns the frame
address, evicting a page if necessary. If successful, returns a pointer to the
frame_table_entry. Otherwise, returns NULL. */
void *
frame_alloc(void *page_addr)
{
  ASSERT(page_addr != NULL);
  lock_acquire(&frame_table_lock);
  struct frame_table_entry *fte = NULL;

  // different threads can map to the same page_addr because each thread has
  // their own page directory. However, a single thread should NOT try to
  // map the same page_addr.
  if (get_frame_table_entry(page_addr) != NULL)
  {
    lock_release(&frame_table_lock);
    return NULL;
  }

  // search for an unused frame in frame table
  for (int i = 0; i < frame_table_size; i++)
  {
    struct frame_table_entry *fte_ = &frame_table[i];
    if (!(fte_->is_mapped))
    {
      fte = fte_;
      break;
    }
  }

  // if all frames in frame_table are in-use, then there is no more space 
  // in user pool and we need to evict one of the pages currently 
  // occupying a frame
  if (fte == NULL)
  {
    if ((fte = frame_evict()) == NULL)
    {
      lock_release(&frame_table_lock);
      return NULL;
    }
  }
  fte->is_mapped = true;
  fte->owner = thread_current();
  fte->spe = get_sup_page_entry(page_addr);
  lock_release(&frame_table_lock);

  return fte;
}

// TODO: might not be needed...
/* Frees fte frame field, removes fte from the frame table, and frees fte. */
void
frame_free(struct frame_table_entry *fte)
{
  ASSERT(lock_held_by_current_thread(&frame_table_lock));
  ASSERT(fte != NULL);
  fte->is_mapped = false;
  fte->owner = NULL;
  fte->spe = NULL;

  // // update clock_hand if fte is before clock_hand in frame_table
  // struct list_elem *e = list_begin(&frame_table);
  // for (int i = 0; e != list_end(&frame_table); e = list_next(e), i++)
  // {
  //   if (e == &fte->frame_elem)
  //   {
  //     if (i <= clock_hand)
  //       clock_hand--;
  //     break;
  //   }
  // }
  // list_remove(&fte->frame_elem);
  // palloc_free_page(fte->frame);
  // free(fte);
}

/* Returns a pointer to the frame_table_entry containing the frame that was
chosen to be evicted by the clock algorithm. If the clock algorithm fails to
find a page eviction candidate, returns NULL. */

void *
frame_evict()
{
  ASSERT(lock_held_by_current_thread(&frame_table_lock));
  // page eviction shouldn't be needed if there are no frames in the frame
  // table to begin with.
  ASSERT(list_size(&frame_table) > 0);

  struct frame_table_entry *fte = NULL;
  // perform clock algorithm starting at clock_hand.
  // should NOT need to iterate through frame_table more than twice to find
  // a page candidate to evict.
  for (int i = clock_hand;
       i < (2 * frame_table_size) + clock_hand;
       clock_hand = (clock_hand + 1) % frame_table_size, i++)
  {
    fte = &frame_table[i];
    if (fte->is_mapped)
    {
      if (pagedir_is_accessed(fte->owner->pagedir, fte->spe->addr))
      {
        pagedir_set_accessed(fte->owner->pagedir, fte->spe->addr, false);
      }
      else
      {
        // skip pinned pages
        if (!(fte->spe->is_pinned))
        {
          clock_hand = (clock_hand + 1) % list_size(&frame_table);
          frame_page_out(fte->spe->addr);
          return fte;
        }
      }
    }
    else
        PANIC("all frames in frame_table should be mapped during eviction");
  }
  PANIC("no page candidate found for eviction");
  return NULL;
}

/* handler for a frame that has been evicted. If page is handled
successfully, returns true. Otherwise, returns false. */
void
frame_page_out(void *page_addr)
{
  ASSERT(lock_held_by_current_thread(&frame_table_lock));
  // int bytes_written = -1;
  struct sup_page_entry *spe = get_sup_page_entry(page_addr);
  struct frame_table_entry *fte = get_frame_table_entry(page_addr);
  ASSERT(spe != NULL && fte != NULL);

  // update present bit, so that next access to uaddr page_addr
  // invokes page_fault handler
  pagedir_clear_page(thread_current()->pagedir, page_addr);
  
  // if spe contains file and is dirty, write back to disk.
  // if spe contains file and is NOT dirty, do nothing.
  if (spe->file != NULL)
  {
    if (pagedir_is_dirty(thread_current()->pagedir, spe->addr))
      file_write_at(spe->file, fte->frame, spe->read_bytes, spe->offset);
  }
  else
  {
    // if spe contains NO file, then write is physical frame to swap parition.
    swap_to_disk(fte);
  }
}