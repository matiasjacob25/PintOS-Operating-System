#include "vm/page.h"
#include "threads/thread.h"
#include "round.h"
#include "palloc.h"
#include "userprog/process.h"
#include "vm/frame.h"

// initializes the supplementary page table.
void sup_page_table_init(struct hash *sup_page_table)
{
  hash_init(&thread_current()->sup_page_table, 
            sup_page_hash, sup_page_less, 
            NULL);
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

/* References data in supplementary page table entry spe to load data into
a newly allocated page starting at virtual address spe->addr. This function
also adds the newly allocated page into the thread's page table and maps it
to a physical frame. Returns true if the page was successfully loaded and 
mapped. Otherwise, returns false. */
bool 
sup_page_load(struct sup_page_entry *spe)
{
  // allocate a physical frame for the page
  int *frame = frame_alloc(spe->addr);

  /*
  Different scenarios of loading data into our pages:
  1. loading data from the swap partition
  2. loading data from a file
  3. loading in an all-zeros page
  */

  // TODO: may need to update the "load from file" case to consider memory-mapped files.
  // load from file (includes executable file)
  if (spe->file != NULL)
  {
    // read data from file into frame, and zero out the rest of 
    // the page, if necessary.
    file_seek(spe->file, spe->offset);
    if (file_read (spe->file, frame, spe->read_bytes) != (int) spe->read_bytes)
    {
      palloc_free_page (frame);
      return false;
    }
    memset (frame + spe->read_bytes, 0, spe->zero_bytes); // can we just use PAL_ZERO flag during palloc_get_page call instead of this?

    // add the page to the process's address space.
    if (!install_page (spe->addr, frame, spe->is_writable)) 
      {
        palloc_free_page (frame);
        return false; 
      }
  }

  // load from swap partition

  // load in an all-zeros page
  if (spe->type == FILE)
  {

  }
}