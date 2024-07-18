#include <list.h>
#include <stdbool.h>

struct frame_table_entry {
  void *frame;
  struct thread *owner;
  void* page;
  // bool reference_bit; // used for page eviction (clock) algorithm
  // bool is_pinned; // used to prevent deadlocks scenarios that can result from page eviction
	struct list_elem frame_elem;
};

void frame_table_init(void);

