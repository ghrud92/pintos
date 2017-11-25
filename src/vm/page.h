#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/off_t.h"
#include "threads/palloc.h"
#include <list.h>

struct page
{
  int table_number;
  int frame_number;
  bool valid_bit;
  struct file* file;
  off_t offset;
  size_t read_bytes;
  size_t zero_bytes;
  bool writable;
  void* upage;
  struct list_elem elem;
};

void init_page_table(struct list*);
void destroy_page_table(struct list*);
bool load_page(struct page* page);
struct page* find_page(void* upage);
bool grow_stack (void * ptr);

#endif /* vm/page.h */
