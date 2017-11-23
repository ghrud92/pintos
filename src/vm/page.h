#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <list.h>

struct page
{
  int table_number;
  int frame_number;
  bool valid_bit;
  struct list_elem elem;
};

void init_page_table(struct list*);
void destroy_page_table(struct list*);
void* get_free_page(void);

#endif /* vm/page.h */
