#include "vm/swap.h"
#include "kernel/bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "threads/synch.h"

/* 
=> 512 bytes/block_sector 
=> 1 page = 4098 bytes = 512bytes * 8 = 8 block_sectors
*/
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

/* A bitmap where each bit represents a single page of memory in the swap
partition. Bit value of 1 indicates that the page (8 block sectors)  */
static struct bitmap *swap_table;

/* handle synchronization during operations on swap table. */
static struct lock swap_table_lock;

/* block_device for BLOCK_SWAP role */
static struct block *block_device;

/* initializes swap table bitmap and lock*/
void
swap_init()
{
  block_device = block_get_role(BLOCK_SWAP);
  if (block_device == NULL)
    NOT_REACHED();  // swap device not found - check test command

  swap_table = bitmap_create(block_size(block_device) / SECTORS_PER_PAGE);
  if (swap_table == NULL)
    NOT_REACHED();  // bitmap_create failed - check test command
  
  lock_init(&swap_table_lock);
}


/* Swaps a page worth of data from physical frame fte to swap partition */
void
swap_to_disk(struct frame_table_entry *fte)
{
  // current thread should be holding frame_table lock since we are modifying
  // contents stored in fte's frame field. 
  ASSERT(lock_held_by_current_thread(&frame_table_lock));
  ASSERT(fte->spe->swap_idx == -1);

  // find first free swap_idx in swap_table
  lock_acquire(&swap_table_lock);
  int swap_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
  lock_release(&swap_table_lock);

  if (swap_idx == BITMAP_ERROR)
    PANIC("Attempting to swap to disk, but swap partition is full.");
  fte->spe->swap_idx = swap_idx;

  // To write a page of data to swap partition, we must write to 8 consecutive
  // block sectors starting at swap_idx*8 (since 1 page == 8 block_sectors)
  for (
    int sector_idx = swap_idx * SECTORS_PER_PAGE; 
    sector_idx < sector_idx + SECTORS_PER_PAGE; sector_idx++)
  {
    block_write(
      block_device, 
      sector_idx, 
      (int *) fte->spe->addr + (sector_idx * BLOCK_SECTOR_SIZE)
    );
  }
}


/* Swaps a page worth of data from swap partition to physical frame fte */
void
swap_from_disk(struct frame_table_entry *fte)
{
  // current thread should be holding frame_table lock since we are modifying
  // contents stored in fte's frame field. 
  ASSERT(lock_held_by_current_thread(&frame_table_lock));
  ASSERT(fte->spe->swap_idx != -1);

  // To read the page of memory stored in swap partition, we must read the 8
  // block sectors starting at spe->swap_idx (since 1 page == 8 block_sectors)
  for (
    int sector_idx = fte->spe->swap_idx * SECTORS_PER_PAGE; 
    sector_idx < sector_idx + SECTORS_PER_PAGE; sector_idx++)
  {
    block_read(
      block_device, 
      sector_idx, 
      (int *) fte->spe->addr + (sector_idx * BLOCK_SECTOR_SIZE)
    );
  }

  // update sup_page_entry and swap_table. Note that we only update swap_table
  // once all memory has been read from swap partition, so that in case of 
  // context switch, another thread does not try to utilize the same swap_idx.
  // TODO: not sure if we need to zero out the swap partition that we just read from???
  lock_acquire(&swap_table_lock);
  bitmap_set(swap_table, fte->spe->swap_idx, false);
  lock_release(&swap_table_lock);
  fte->spe->swap_idx = -1;
}