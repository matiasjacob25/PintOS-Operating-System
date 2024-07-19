// Purpose of swap table is to provide the OS with information about what
// sectors in the swap partition are in use. Thus, this can be done with
// minimal overhead by using a bitmap, which is important since swap table is
// global

void swap_init(void);
void swap_to_disk(struct frame_table_entry *fte);
void swap_from_disk(struct frame_table_entry *fte);

