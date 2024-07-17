#include "vm/page.h"
#include "threads/thread.h"

// initializes the supplementary page table.
void
sup_page_table_init (struct hash *sup_page_table)
{
  hash_init(&thread_current()->sup_page_table, sup_page_hash, sup_page_less, NULL);
}

/* constructor function for supplementary page table entry */
// void 
// sup_page_entry_init (
//   struct sup_page_entry *spe, 
//   void* uaddr, 
//   enum page_type type, 
//   uint32_t read_bytes, 
//   uint32_t zero_bytes, 
//   struct file *file, 
//   off_t offset, 
//   int swap_idx, 
//   bool is_in_memory) 
// {
//   spe->uaddr = uaddr;
//   spe->type = type;
//   spe->read_bytes = read_bytes;
//   spe->zero_bytes = zero_bytes;
//   spe->file = file;
//   spe->offset = offset;
//   spe->swap_idx = swap_idx;
//   spe->is_in_memory = is_in_memory; 
// }

// Returns a pointer to the sup_page_entry that corresponds to a page that 
// contains user address uaddr. Otherwise, returns NULL.
struct sup_page_entry*
get_sup_page_entry(void *uaddr) {
}

// returns a hash key that corresponds to hash_elem h
unsigned
sup_page_hash (const struct hash_elem *h, void *aux)
{
  const struct sup_page_entry *spe = hash_entry (
    h, 
    struct sup_page_entry, 
    sup_hash_elem
  );
  return hash_bytes (spe->addr, sizeof spe->addr);
}

/* Returns true if page a's address is smaller than page b's. */
bool
sup_page_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux)
{
  const struct sup_page_entry *a = hash_entry (a_, struct sup_page_entry, sup_hash_elem);
  const struct sup_page_entry *b = hash_entry (b_, struct sup_page_entry, sup_hash_elem);
  return a->addr < b->addr;
}