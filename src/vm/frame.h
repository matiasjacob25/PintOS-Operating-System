#include <list.h>
#include <stdbool.h>
#include "vm/page.h"

/* handle synchronization during operations on frame table. */
struct lock frame_table_lock;

struct frame_table_entry {
  /* physical frame address */
  void *frame;
  /* thread that is using this frame */
  struct thread *owner;
  /* reference to the sup_page_entry that corresponds to this frame */
  struct sup_page_entry *spe;
  // bool reference_bit; // used for page eviction (clock) algorithm
  // bool is_pinned; // used to prevent deadlocks scenarios that can result from page eviction
	struct list_elem frame_elem;
};

void frame_table_init(void);
void * frame_alloc (void *page);
void frame_free (struct frame_table_entry *fte);
void* frame_evict();

