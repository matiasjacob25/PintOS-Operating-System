#include "vm/frame.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"

/* List of all frames occupying space in physical memory. */
static struct list frame_table;

/* Clock hand for page eviction (clock) algorithm. */
static int clock_hand = 0;

/* initializes frame table list and lock*/
void
frame_table_init () 
{
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}

/* Allocates a frame for the page and returns the frame address, evicting a 
page if necessary. If successful, returns a pointer to the frame address. 
Otherwise, returns NULL. */
void *
frame_alloc (void *page) {
  ASSERT(page != NULL);
  lock_acquire(&frame_table_lock);

  void *frame = palloc_get_page (PAL_USER);

  // if palloc_get_page fails, then there is no more space in user pool,
  // and we need to evict one of the pages currently occupying a frame
  if (frame == NULL) {
      if ((frame = frame_evict()) == NULL)
      {
        lock_release(&frame_table_lock);
        return NULL;
      }
  }

  struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));
  fte->frame = frame;
  fte->owner = thread_current();
  fte->spe = get_sup_page_entry(page);
  list_push_back (&frame_table, &fte->frame_elem);

  lock_release(&frame_table_lock);
  return frame;
}

/* Frees fte frame field, removes fte from the frame table, and frees fte. */
void
frame_free (struct frame_table_entry *fte) {
  ASSERT(fte != NULL);

  lock_acquire(&frame_table_lock);
  list_remove(&fte->frame_elem);
  palloc_free_page(fte->frame);
  free(fte);
  lock_release(&frame_table_lock);
}

/* Evicts a page from the frame table using the clock algorithm. The evicted 
page will be saved to swap partition, if necessary. If successful, returns 
the frame address that has been freed. Otherwise, returns NULL.
*/
void * 
frame_evict() {
  // page eviction shouldn't be needed if there are no frames in the frame 
  // table to begin with.
  ASSERT(size(&frame_table) > 0);

  //TODO: implement clock algorithm
  if (clock_hand == size(&frame_table))
    clock_hand = 0;
  

  // file-loaded pages should be written back to file system, if dirty.

  // non file-loaded pages should be written to swap partition.
  swap_to_disk(fte);
}