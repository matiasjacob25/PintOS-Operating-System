#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  return file_open (get_inode_from_path(name));
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

// TODO: add handling for special cases like ".." and "."
/* Returns a pointer to the inode of the last token in PATHNAME. 
Returns NULL if the inode for the last token in PATHNAME does not exist.
Note that the returned inode may correspond to a file OR a directory. */
struct inode *
get_inode_from_path(char *pathname)
{
  ASSERT (pathname != NULL);
  char *token, *next_token, *save_ptr;
  struct dir *dir = NULL;
  struct inode *inode = NULL;

  if (strlen(pathname) == 0)
    return NULL;

  // determine relative versus absolute path.
  if (pathname[0] == "/")
    dir = dir_open_root();
  else
    dir = thread_current()->cwd;

  // traverse and validate each directory in the pathname, up until
  // the final directory_name/file_name token.
  token = strtok_r (pathname, "/", &save_ptr);
  while (token != NULL)
  {
    next_token = strtok_r (NULL, "/", &save_ptr);
    // handle final directory_name/file_name token.
    if (next_token == NULL)
    {
      if (dir == NULL)
        PANIC("Invalid directory");
      dir_lookup (dir, token, &inode);
      dir_close (dir);
      return inode;
    }
    // handle intermediate pathname tokens (should all be directories).
    dir_lookup(dir, token, &inode);
    dir_close(dir);
    
    if (inode->is_dir == false)
      PANIC("Intermidate pathname is NOT a directory");
    else
    {
      dir = dir_open(inode);
      token = next_token;
    }
  }
  return NULL;
}