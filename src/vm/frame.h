#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/thread.h"
#include <list.h>

struct frame
{
  int holder;
  void * paddr;
  void * vaddr;
  unsigned int page_directory;
  bool valid_bit;
  bool writable;
  struct list_elem elem;
  //
  int frame_number;
  void* memory;
};

struct list frame_table;

void init_table(void);  // initialize frame table
void* get_free_frame(enum palloc_flags, void* upage); // find a free frame
void free_frame(void*);  // free an existing frame
struct frame *number_to_frame (tid_t finding_no);  // find frame that has the frame number

#endif /* vm/frame.h */
