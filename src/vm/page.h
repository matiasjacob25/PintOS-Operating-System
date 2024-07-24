#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stddef.h>
#include <inttypes.h>
#include "filesys/off_t.h"
#include <stdbool.h>
#include <hash.h>

/* provides additional information about a page of data */
struct sup_page_entry {
  /* virtual address (in user address space) of start of the page */
  void *addr;
  /* if true, the page can be modified. Otherwise, read-only. */
  bool is_writable;

  /* file that the page reads from */
  struct file *file;
  /* offset that the page reads from in the file */
  off_t offset; 
  /* number of bytes that should be read from file and stored in the page */
  uint32_t read_bytes;
  /* number of bytes that should be zeroed out in the page */
  uint32_t zero_bytes;

  /* index of page in swap table */
  int swap_idx; 
  /* hash_elem */
  struct hash_elem sup_hash_elem;

  /* whether page is currently being pinned */
  bool is_pinned;
};

void sup_page_table_init (struct hash *sup_page_table);
struct sup_page_entry* get_sup_page_entry(void *addr);
void sup_page_free(void* page_addr);
unsigned sup_page_hash(const struct hash_elem *h, void *aux);
bool sup_page_less(const struct hash_elem *a_, 
                   const struct hash_elem *b_,
                   void *aux);
bool sup_page_load(struct sup_page_entry *spe);
void sup_page_free(void* page_addr);
void page_pin(void *addr);
void page_unpin(void *addr);

#endif /* vm/page.h */