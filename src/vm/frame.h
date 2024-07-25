#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>
#include "vm/page.h"

/* handle synchronization during operations on frame table. */
struct lock frame_table_lock;

/* List of all frames occupying space in physical memory. */
struct frame_table_entry *frame_table;

/* number of frames in the frame table */
int frame_table_size;

struct frame_table_entry {
  /* physical frame address */
  void *frame;
  /* thread that is using this frame */
  struct thread *owner;
  /* reference to the sup_page_entry that corresponds to this frame */
  struct sup_page_entry *spe;
  /* whether or not some user page is currently mapped to this frame */
  bool is_mapped;
	struct list_elem frame_elem;
};

void frame_table_init(void);
struct frame_table_entry * get_frame_table_entry(void* addr_);
void * frame_alloc (void *page);
void frame_free (struct frame_table_entry *fte);
void* frame_evict();
void frame_page_out(void* page_addr);

#endif /* vm/frame.h */