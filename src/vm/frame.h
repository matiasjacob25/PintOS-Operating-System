#include <list.h>
#include <stdbool.h>
#include "vm/page.h"

/* List of all frames occupying space in physical memory. */
struct list frame_table;

/* handle synchronization during operations on frame table. */
struct lock frame_table_lock;

struct frame_table_entry {
  /* physical frame address */
  void *frame;
  /* thread that is using this frame */
  struct thread *owner;
  /* reference to the sup_page_entry that corresponds to this frame */
  struct sup_page_entry *spe;
  // bool is_pinned; // used to prevent deadlocks scenarios that can result from page eviction
	struct list_elem frame_elem;
};

void frame_table_init(void);
struct frame_table_entry * get_frame_table_entry(void* addr_);
void * frame_alloc (void *page);
void frame_free (struct frame_table_entry *fte);
void* frame_evict();
void frame_page_out(void* page_addr);

