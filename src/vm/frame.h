#include <list.h>
#include <stdbool.h>

struct frame_table_entry {
  // void* physical_addr
  struct thread * owner;
  struct sup_page_entry * spe;
  bool reference_bit; // used for page eviction (clock) algorithm
  bool is_pinned; // used to prevent deadlocks scenarios that can result from page eviction
	// struct list_elem elem;
};

void frame_table_init(void)

