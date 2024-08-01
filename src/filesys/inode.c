#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Max number of pointers that a single block can contain. */
#define PTRS_PER_BLOCK 128

/* Number of blocks stored in each type of indexed blocks. */
#define DIRECT_SIZE 1
#define INDIRECT_SIZE 128
#define DINDIRECT_SIZE  (128 * 128)

/* Number of bytes stored in each type of indexed blocks. */
#define MAX_DIRECT_BYTE (DIRECT_SIZE * 512)
#define MAX_INDIRECT_BYTE (INDIRECT_SIZE * 512)
#define MAX_DINDIRECT_BYTE (DINDIRECT_SIZE * 512)

/* Number of block pointers that can be used for each block pointer type. */
#define DIRECT_PTRS 8
#define INDIRECT_PTRS 1
#define DINDIRECT_PTRS 1

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[112];               /* Not used. */

    block_sector_t blocks[10];          /* Block pointers. */
    uint32_t direct_index;              /* Direct block index. */
    uint32_t indirect_index;            /* Indirect block index. */
    uint32_t d_indirect_index;          /* Double indirect block index. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */

    struct lock lock;                   /* Inode lock. */
    off_t length;                       /* File size in bytes. */
    block_sector_t blocks[10];          /* Block pointers. */
    uint32_t direct_index;              /* Direct block index. */
    uint32_t indirect_index;            /* Indirect block index. */
    uint32_t d_indirect_index;          /* Double indirect block index. */
  };

/* define static functions */
static bool 
inode_free_db_indirect(struct inode *inode, uint32_t sectors_left);
static bool 
inode_free_indirect(struct inode *inode, uint32_t sectors_left);
static bool
inode_grow_db_indirect (struct inode *inode, uint32_t *sectors_left);
static bool
inode_grow_indirect(struct inode *inode, uint32_t *sectors_left);
static bool 
inode_grow (struct inode *inode, off_t length);
static bool
inode_alloc (struct inode_disk *disk_inode, off_t length);
// static bool inode_create_indirect (...);
// static bool inode_create_db_indirect (...);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  int sector, direct_index, indirect_index, d_indirect_index;
  block_sector_t block_ptrs[PTRS_PER_BLOCK];

  if (pos < inode->data.length)
  {
    // sector number exists within direct block pointers.
    if (pos < DIRECT_PTRS * MAX_DIRECT_BYTE)
    {
      direct_index = pos / MAX_DIRECT_BYTE;
      sector = inode->blocks[direct_index];
    }

    // sector number exists within indirect block pointers.
    else if (pos < (DIRECT_PTRS * MAX_DIRECT_BYTE) + 
            (INDIRECT_PTRS * MAX_INDIRECT_BYTE))
    {
      indirect_index = (pos - DIRECT_PTRS * MAX_DIRECT_BYTE) / 
                       MAX_DIRECT_BYTE;
      block_read(fs_device, inode->blocks[DIRECT_PTRS], &block_ptrs);
      sector = block_ptrs[indirect_index];
    }

    // TODO: finish implementation of d_indirect's byte_to_sector
    // // sector number exists within double indirect block pointers.
    // else 
    // {
    //   d_indirect_index = (pos - (DIRECT_PTRS * MAX_DIRECT_BYTE) -
    //                      (INDIRECT_PTRS * MAX_INDIRECT_BYTE)) %
    //                       MAX_INDIRECT_BYTE;
    //   indirect_index = ()
    //   indirect_index = (pos -(DIRECT_PTRS * MAX_DIRECT_BYTE) -
    //                     (INDIRECT_PTRS * MAX_INDIRECT_BYTE)) %
    //                     MAX_INDIRECT_BYTE;
    // }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      size_t sectors = bytes_to_sectors (length);

      // initialize length bytes worth of sectors using inode's block pointers,
      // then store the updated disk_inode to disk at sector sector.
      if (inode_alloc(disk_inode, length))
      {
        block_write (fs_device, sector, disk_inode);
        success = true;
      }
      free(disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  // update fields shared by disk_inode and inode.
  block_read (fs_device, inode->sector, &inode->data);
  struct inode_disk *disk_inode = &inode->data;
  inode->length = disk_inode->length;
  inode->direct_index = disk_inode->direct_index;
  inode->indirect_index = disk_inode->indirect_index;
  inode->d_indirect_index = disk_inode->d_indirect_index;
  memcpy(&inode->blocks, &disk_inode->blocks, 
  (DIRECT_PTRS + INDIRECT_PTRS + DINDIRECT_PTRS) * sizeof(block_sector_t));
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          uint32_t sectors_left = bytes_to_sectors(inode->length);

          // deallocate sector containing inode metadata (struct fields).
          free_map_release (inode->sector, 1);

          // deallocate inode's direct block pointers.
          for (int i = 0; i < inode->direct_index && sectors_left > 0; i++)
            free_map_release(inode->blocks[i], 1);

          if (sectors_left > 0)
            // deallocate inode's indirect block pointers.
            if (!inode_free_indirect(&inode, &sectors_left))
              PANIC("Error deallocating indirect block pointers.");

          if (sectors_left > 0)
            // deallocate inode's double indirect block pointers. 
            if (!inode_free_db_indirect(&inode, &sectors_left))
              PANIC("Error deallocating double indirect block pointers.");
        }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

// Allocates length bytes worth of sectors for a new inode with disk 
// inode disk_inode. Returns true if sectors are allocated successfully. 
// Otherwise returns false.
static bool
inode_alloc (struct inode_disk *disk_inode, off_t length)
{
  bool success = false;

  // run inode_grow on newly initialzied inode so that we only update
  // disk_inode's block pointers if inode_grow is successful.
  struct inode inode;
  inode.length = 0;
  inode.direct_index = 0;
  inode.indirect_index = 0;
  inode.d_indirect_index = 0;

  if (inode_grow(&inode, length))
  {
    disk_inode->direct_index = inode.direct_index;
    disk_inode->indirect_index = inode.indirect_index;
    disk_inode->d_indirect_index = inode.d_indirect_index;
    memcpy(&disk_inode->blocks, 
          &inode.blocks, 
          (DIRECT_PTRS + INDIRECT_PTRS + DINDIRECT_PTRS) * 
          sizeof(block_sector_t));
    success = true;
  }

  return success;
}

// Allocates length bytes worth of sectors for inode inode and updates inodes
// block pointers accordingly. Returns true if allocated succesfully.
// Otherwise, returns false. 
static bool inode_grow (struct inode *inode, off_t length) {
  static char zeros[BLOCK_SECTOR_SIZE];
  // only want to allocate new sectors for data that's extending the inode's 
  // previous length.
  uint32_t sectors_left = bytes_to_sectors(length);
  
  // there is still room in existing sectors to write the new data.
  if (sectors_left == 0) 
  {
    return true;
  }

  // Allocate new zeroed sectors for new inode data.
  // First, fill up empty direct block ptrs first. 
  while (inode->direct_index < DIRECT_PTRS && sectors_left > 0) 
  {
    if (free_map_allocate(1, &inode->blocks[inode->direct_index]))
    {
      block_write(fs_device, inode->blocks[inode->direct_index], zeros);
      inode->direct_index++;
      sectors_left--;
    }
    else
      return false;
  }

  // Next, fill up empty indirect block ptrs (if necessary).
  // while (inode->indirect_index < INDIRECT_PTRS && sectors_left > 0)
  if (inode->indirect_index < INDIRECT_PTRS && sectors_left > 0)
    inode_grow_indirect(inode, &sectors_left);

  // Next, fill up empty indirect block ptrs (if necessary).
  if ( inode->d_indirect_index < DINDIRECT_SIZE && sectors_left > 0)
    inode_grow_db_indirect(inode, &sectors_left);
  
  return true;
}

// Allocates at most sectors_left sectors using inode inode's indirect block 
// pointers. Returns true if memory allocation is successful. Otherwise returns
// false. 
static bool
inode_grow_indirect(struct inode *inode, uint32_t *sectors_left) {
  static char zeros[BLOCK_SECTOR_SIZE];
  block_sector_t indirect_block[PTRS_PER_BLOCK];

  // should expect direct block ptrs to be full.
  ASSERT(inode->direct_index == DIRECT_PTRS);

  // allocate space to store indirect block's pointers if it hasn't been 
  // allocated yet. Otherwise, read in the indirect blocks from disk.
  if (inode->indirect_index == 0)
  {
    if (!free_map_allocate(1, &inode->blocks[DIRECT_PTRS]))
      return false;
  }
  else
    block_read(fs_device, &inode->blocks[DIRECT_PTRS], &indirect_block);
  
  // allocate the sectors for block pointers in the indirect block.
  while (inode->indirect_index < PTRS_PER_BLOCK && *sectors_left > 0)
  {
    if (free_map_allocate(1, &indirect_block[inode->indirect_index]))
    {
      block_write(fs_device, indirect_block[inode->indirect_index], zeros);
      inode->indirect_index++;
      *sectors_left = *sectors_left - 1;
    }
    else
      return false;
  }

  // write updates made to indirect block's pointers back to disk.
  block_write(fs_device, inode->blocks[DIRECT_PTRS], &indirect_block);
  return true;
}

// Allocates at most sectors_left sectors using inode inode's doubly indirect
// block pointers. Returns true if memory allocation is successful. Otherwise 
// returns false.
static bool
inode_grow_db_indirect (struct inode *inode, uint32_t *sectors_left)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  block_sector_t d_indirect_block[PTRS_PER_BLOCK][PTRS_PER_BLOCK];
  block_sector_t indirect_block[PTRS_PER_BLOCK];
  uint32_t d_indirect_index;
  uint32_t indirect_index;
  
  // should expect indirect block ptrs to be full.
  ASSERT(inode->direct_index == INDIRECT_SIZE);

  // allocate space to store double indirect block's pointers if it hasn't been 
  // allocated yet. Otherwise, read the double indirect block from disk.
  if (inode->d_indirect_index == 0)
  {
    if (!free_map_allocate(1, &inode->blocks[DIRECT_PTRS + INDIRECT_PTRS]))
      return false;
  }
  else
    block_read(fs_device, inode->blocks[DIRECT_PTRS + INDIRECT_PTRS], 
               &d_indirect_block);

  while (inode->d_indirect_index < DINDIRECT_SIZE && *sectors_left > 0)
  {
    // allocate space for new indirect block if it hasn't been allocated yet.
    // Otherwise, read the indirect block from disk.
    int d_indirect_index = inode->d_indirect_index / PTRS_PER_BLOCK;
    int indirect_index = inode->d_indirect_index % PTRS_PER_BLOCK;
    if (indirect_index == 0)
    {
      if (!free_map_allocate(1, 
      &d_indirect_block[d_indirect_index][indirect_index]))
        return false;
    }
    else
      block_read(fs_device, d_indirect_block[d_indirect_index][indirect_index],
                  &indirect_block);

    while (indirect_index < PTRS_PER_BLOCK && *sectors_left > 0)
    {
      if (free_map_allocate(1, &indirect_block[indirect_index]))
      {
        block_write(fs_device, indirect_block[indirect_index], zeros);
        indirect_index++;
        *sectors_left--;
      }
      else
        return false;

      // write updates made to indirect block's pointers back to disk.
      // block_write(fs_device, inode->blocks[DIRECT_PTRS], &indirect_block);
    }

  // write updates made to double indirect block's pointers back to disk.
  }

  return true;
}

/* Precondition: direct blocks have already been freed. */
static bool 
inode_free_indirect(struct inode *inode, uint32_t sectors_left)
{
  ASSERT(sectors_left > 0);
  block_sector_t indirect_block[PTRS_PER_BLOCK];
  uint32_t cur_indirect_index = 0;
  bool success;

  // load indirect block from disk.
  block_read(fs_device, inode->blocks[DIRECT_PTRS], &indirect_block);

  // release pointers in indirect block that are occupied in the free_map.
  while (cur_indirect_index < inode->indirect_index && sectors_left > 0);
  {
    free_map_release(indirect_block[cur_indirect_index], 1);
    cur_indirect_index++;
    sectors_left--;
  }

  // release indirect block,
  free_map_release(inode->blocks[DIRECT_PTRS], 1);
  return true;
}

// TODO: finish implementation...
/* Precondition: direct and indirect blocks have already been freed. */
static bool 
inode_free_db_indirect(struct inode *inode, uint32_t sectors_left)
{
  ASSERT(sectors_left > 0);

  // block_sector_t d_indirect_block[PTRS_PER_BLOCK][PTRS_PER_BLOCK];
  // block_sector_t indirect_block[PTRS_PER_BLOCK];
  // uint32_t cur_d_indirect_index = 0;
  // uint32_t cur_indirect_index = 0;
  // bool success;

  // // load double indirect block from disk.
  // block_read(fs_device, inode->blocks[DIRECT_PTRS + INDIRECT_PTRS], 
  //            &d_indirect_block);

  // // release pointers in double indirect block that are occupied in the free_map.
  // while (cur_d_indirect_index < inode->d_indirect_index && sectors_left > 0)
  // {
  //   // load indirect block from disk.
  //   block_read(fs_device, d_indirect_block[cur_d_indirect_index][cur_indirect_index], 
  //              &indirect_block);

  //   while (cur_indirect_index < PTRS_PER_BLOCK && sectors_left > 0)
  //   {
  //     if (free_map_release(indirect_block[cur_indirect_index], 1))
  //     {
  //       cur_indirect_index++;
  //       sectors_left--;
  //     }
  //     else
  //       return false;
  //   }

  //   // release indirect block,
  //   if (!free_map_release(d_indirect_block[cur_d_indirect_index][cur_indirect_index], 1))
  //     return false;
  //   cur_d_indirect_index++;
  // }

  // // release double indirect block,
  // if (!free_map_release(inode->blocks[DIRECT_PTRS + INDIRECT_PTRS], 1))
  //   return false;
  // return true;
}

