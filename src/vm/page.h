#ifndef VM_PAGE_H
#define VM_PAGE_H

struct page
{
  int table_number;
  int frame_number;
  bool valid_bit;
  struct list_elem elem;
  struct hash ha;
};

struct list page_table;

void init_table(void);
void* get_free_page();

#endif /* vm/page.h */
