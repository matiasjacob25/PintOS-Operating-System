#include "vm/page.h"

/* constructor function for supplementary page table entry */
void 
page_entry_init (
  struct sup_page_entry *spe, 
  void* uaddr, 
  enum page_type type, 
  uint32_t read_bytes, 
  uint32_t zero_bytes, 
  struct file *file, 
  off_t offset, 
  int swap_idx, 
  bool is_in_memory) 
{
  spe->uaddr = uaddr;
  spe->type = type;
  spe->read_bytes = read_bytes;
  spe->zero_bytes = zero_bytes;
  spe->file = file;
  spe->offset = offset;
  spe->swap_idx = swap_idx;
  spe->is_in_memory = is_in_memory; 
}