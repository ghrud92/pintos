#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/thread.h"
#include <list.h>

struct frame
{
  int frame_number;
  void* memory;
  int tid;
  struct list_elem elem;
};

struct list frame_table;

void init_table(void);  // initialize frame table
void* get_free_frame(enum palloc_flags, void* upage); // find a free frame
void free_frame(void*);  // free an existing frame
struct frame *number_to_frame (tid_t finding_no);  // find frame that has the frame number

#endif /* vm/frame.h */
