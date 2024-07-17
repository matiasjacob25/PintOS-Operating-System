#include "vm/page.h"
#include "threads/thread.h"
#include "round.h"
#include "palloc.h"

// initializes the supplementary page table.
void sup_page_table_init(struct hash *sup_page_table)
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

// Returns a pointer to the sup_page_entry that corresponds to a page containing the
// virtual address addr.
// Otherwise, returns NULL.
struct sup_page_entry *
get_sup_page_entry(void *addr)
{
  struct sup_page_entry spe;
  struct hash_elem *e;

  spe.addr = (void *)pg_round_down(addr);
  e = hash_find(&thread_current()->sup_page_table, &spe);

  if (e != NULL)
    return hash_entry(e, struct sup_page_entry, sup_hash_elem);
  return NULL;
}

// returns a hash key that corresponds to hash_elem h
unsigned
sup_page_hash(const struct hash_elem *h, void *aux)
{
  const struct sup_page_entry *spe = hash_entry(
      h,
      struct sup_page_entry,
      sup_hash_elem);
  return hash_bytes(spe->addr, sizeof spe->addr);
}

/* Returns true if page a's address is smaller than page b's. */
bool sup_page_less(const struct hash_elem *a_, const struct hash_elem *b_,
                   void *aux)
{
  const struct sup_page_entry *a = hash_entry(a_, struct sup_page_entry, sup_hash_elem);
  const struct sup_page_entry *b = hash_entry(b_, struct sup_page_entry, sup_hash_elem);
  return a->addr < b->addr;
}

// References data in supplementary page table entry spe to load data into
// a newly allocated page starting at virtual address spe->addr. This function
// also adds the newly allocated page into the thread's page table and maps it
// to a physical frame.
//
sup_page_load(struct sup_page_entry *spe)
{
  // if palloc_get_page fails, then there is no more space in user pool,
  // and we need to evict one of the pages that were previously allocated
  // using palloc_get_page(PAL_USER)
  uint8_t *kpage = palloc_get_page(PAL_USER);
  if (kpage == NULL)
    // TODO: frame_evict should be some eviction policy that returns the
    // kpage laddress that has become available
    // swapping should be handled by this function as well.
    kpage = frame_evict();

  frame_allocate();
  if (spe->type == FILE)
  {
  }

  /*
  Different scenarios of loading data into our pages:
  1. loading data from the swap partition
  2. loading data from a file
  3. loading in an all-zeros page


  */
  // load the page
  // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
  //   {
  //     palloc_free_page (kpage);
  //     return false;
  //   }
  // memset (kpage + page_read_bytes, 0, page_zero_bytes);

  // /* Add the page to the process's address space. */
  // if (!install_page (upage, kpage, writable))
  //   {
  //     palloc_free_page (kpage);
  //     return false;
  //   }
}