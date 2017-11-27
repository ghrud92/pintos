#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

int frame_number = 0;

struct frame* find_free_frame(enum palloc_flags);

void init_table()
{
  static bool init = true;
  if (init)
  {
    list_init(&frame_table);
    init = false;
  }
}

void* get_free_frame(enum palloc_flags flags, void* vaddr)
{
  init_table();
  
  if ((flags & PAL_USER) == 0)
  {
    return NULL;
  }
  void * page_addr = palloc_get_page(flag);
  if(paddr == NULL)
  {
    paddr = frame_eviction(flag);
  }
  struct frame f;
  f -> holder = thread_current()->tid;
  f -> paddr = paddr;
  f -> vaddr = vaddr;
  f -> pagedir = thread_current() -> pagedir;
  f -> writable = true;
  f -> valid_bit = true;
  list_push_back (&frame_list, &(f->elem));
  frame_number++;
  /*
  struct frame* frame = find_free_frame(flags);
  if (frame != NULL)
  {
      frame -> tid = thread_tid();
      frame -> memory = palloc_get_page (flags);
      // install_page (upage, frame -> memory, true);
      return frame -> memory;
  }

  void* memory = palloc_get_page(flags);
  // install_page (upage, memory, true);
  frame = malloc (sizeof(struct frame));
  frame -> frame_number = frame_number;
  frame -> memory = memory;
  frame -> tid = thread_tid();
  list_push_back (&frame_table, &frame->elem);

  

  return frame -> memory;
  */
}
/*
struct frame* find_free_frame(enum palloc_flags flags)
{
  if (list_empty(&frame_table))
      return NULL;

  struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);
  while (now != NULL)
  {
    if (now -> memory == NULL)
    {
      return now;
    }

    if (now -> elem.next != NULL)
        now = list_entry(list_next(&(now->elem)), struct frame, elem);
    else
        break;
  }
  return NULL;
}
*/

void free_frame(void* target_paddr)
{
    struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);
    while (now != NULL)
    {
        if (now -> paddr == target_paddr)
        {
          now -> valid_bit = false;
          list_remove(&(f->elem));
          palloc_free_page (paddr);
          break;
        }
        if (now -> elem.next != NULL)
            now = list_entry(list_next(&(now->elem)), struct frame, elem);
        else
            break;
    }
}

void * frame_eviction (enum palloc_flags flag)
{
  struct frame * f = list_entry (list_pop_front (&frame_list), struct frame, elem);
  swap_to_disk (f);
  pagedir_clear_page (f -> pagedir, f -> vaddr);
  palloc_free_page (f -> paddr);
  list_remove (&(f -> elem));
  free(f);
  return palloc_get_page(flag);
}

struct frame *
number_to_frame (tid_t finding_no)
{
    printf("%s %d\n", "tid", thread_current ()->tid);
  if (list_empty(&frame_table))
    return NULL;
  struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);

  while (now != NULL)
  {
      printf("%s %p\n", "frame table address", now);
    if (now -> frame_number == finding_no)
    {
      return now;
    }
    if (now == list_entry(list_end(&frame_table), struct frame, elem))
      break;
    now = list_entry(list_next(&(now->elem)), struct frame, elem);
  }
  return NULL;
}
