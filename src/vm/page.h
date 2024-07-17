#include <stddef.h>
#include <inttypes.h>
#include <off_t.h>
#include <stdbool.h>
#include <hash.h>

/* handle synchronization during operations on supplementary page table. */
struct lock sup_page_table_lock;

/* type of data that is represented by a page_entry*/
enum page_type {
  FILE,
  STACK,      
  EXECUTABLE
};

/* provides additional information about a page of memory */
struct sup_page_entry {
  /* virtual address of start of the page*/
  void *addr;
  /* type of data that the page corresponds to */
  enum page_type type;
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
  /* whether or not page exists in physical memory */
  bool is_in_memory;
  /* hash_elem */
  struct hash_elem sup_hash_elem;
};

// void sup_page_entry_init(
//   struct sup_page_entry *spe, 
//   void* uaddr, 
//   enum page_type type, 
//   uint32_t read_bytes, 
//   uint32_t zero_bytes, 
//   struct file *file, 
//   off_t offset, 
//   int swap_idx, 
//   bool is_in_memory
// );

void sup_page_table_init (void);


/*
Sample workflow:
- page table entries as well as supplemental page table entries are
  created when files are mapped, or when executable data is loaded during
  load_segment()
    - when setup_stack() is called to initialize the stack, can we still 
      assume that the stack will only be at most 1 page long?
- program tries to access a vaddr that is not in physical memory
= page fault occurs
- page fault handler is invoked
- page fault handler determines that vaddr is valid
- page fault handler determines the pagetype of vaddr and loads the page into
  physical memory accordingly
  - mmap file:
    - use supplementary page table or mapping table to find the bytes of a file
      that should be copied over to the page address that caused the page 
      fault. Then, load the file data into physical memory, and update the 
      mapping between page table entry (PTE) and supplementary page table 
      entry (SPE)
  - executable (does this include the code, BSS and other non-stack segments?): 
    - similar process to mmap file?

  - stack: 
    - validate stack pointer via some heuristic (less than 32 bytes from f->esp)
      - PUSH pushes 4 bytes at once and PUSHA pushed 32 bytes at once in PINTOS.
        Thus, as long as (fault_addr >= f->esp - 32) we can account for 
        of 
    - if approved for stack-growth, create a new PTE and SPE for the new page, 
      as well as map the PTE to the frame table 

*/
