#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_init(void);
void swap_to_disk(struct frame_table_entry *fte);
void swap_from_disk(struct frame_table_entry *fte);

#endif /* vm/swap.h */