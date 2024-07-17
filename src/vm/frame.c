#include "vm/frame.h"

/* List of all frames occupying space in physical memory. */
struct list frame_table;

/* handle synchronization during operations on frame table. */
struct lock frame_table_lock;

/* initializes frame table list and lock*/
void
frame_table_init () 
{
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}



void *
frame_alloc (enum palloc_flags flags, void *page) 
{
  lock_acquire(&frame_table_lock);
  

}

// Jerry's Code------------------------
// void *
// frame_alloc (enum palloc_flags flags, void *page) {
//     lock_acquire(&frame_table_lock);

//     void *frame = palloc_get_page (PAL_USER | flags);
//     if (frame == NULL) {
//         lock_release(&frame_table_lock);
//         return NULL; // Allocation failed, no swapping for now
//     }

//     struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));
//     fte->frame = frame;
//     fte->owner = thread_current ();
//     fte->page = page;
//     hash_insert (&frame_table, &fte->hash_elem);

//     lock_release(&frame_table_lock);
//     return frame;
// }

// void
// frame_free (void *frame) {
//     lock_acquire(&frame_table_lock);

//     struct frame_table_entry fte;
//     fte.frame = frame;
//     struct hash_elem *e = hash_delete (&frame_table, &fte.hash_elem);
//     if (e != NULL) {
//         struct frame_table_entry *fte = hash_entry (e, struct frame_table_entry, hash_elem);
//         palloc_free_page (fte->frame);
//         free (fte);
//     }

//     lock_release(&frame_table_lock);
// }