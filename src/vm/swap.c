#include "vm/swap.h"
#include "kernel/bitmap.h"
#include "devices/block.h"
#include "vaddr.h"

/* 
=> 512 bytes/block_sector 
=> 1 page = 4098 bytes = 512bytes * 8 = 8 block_sectors
*/
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* A bitmap where each bit represents a single page of memory in the swap
partition. Bit value of 1 indicates that the page (8 block sectors)  */
struct bitmap *swap_table;

/* handle synchronization during operations on swap table. */
struct lock swap_table_lock;

/* initializes swap table bitmap and lock*/
void
swap_init()
{
  struct block *block_device = block_get_role(BLOCK_SWAP);
  if (block_device == NULL)
    NOT_REACHED();  // swap device not found - check test command

  swap_table = bitmap_create(block_size(block_device) / SECTORS_PER_PAGE);
  if (swap_table == NULL)
    NOT_REACHED();  // bitmap_create failed - check test command
  
  lock_init(&swap_table_lock);
}

