#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/off_t.h"
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
  uint8_t* upage;
  struct list_elem elem;
};

void init_page_table(struct list*);
void destroy_page_table(struct list*);
bool load_page(struct page* page);
struct page* get_page(uint8_t* upage);

#endif /* vm/page.h */
