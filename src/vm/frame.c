#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"

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

void* get_free_frame(enum palloc_flags flags)
{
  init_table();

  if ((flags & PAL_USER) == 0)
  {
    return NULL;
  }

  struct frame* frame = find_free_frame(flags);
  if (frame != NULL)
  {
      frame -> tid = thread_tid();
      return frame -> memory;
  }

  void* memory = palloc_get_page(flags);
  frame = malloc (sizeof(struct frame));
  frame -> frame_number = frame_number;
  frame -> memory = memory;
  frame -> tid = thread_tid();
  list_push_back (&frame_table, &frame->elem);

  frame_number++;

  return memory;
}

struct frame* find_free_frame(enum palloc_flags flags)
{
  if (list_empty(&frame_table))
      return NULL;
  struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);
  while (now != NULL)
  {
    if (now -> memory == NULL)
    {
      temp -> memory == NULL;
      return temp;
    }
    else
      return NULL;
    if (now == list_entry(list_end(&frame_table), struct frame, elem))
      break;
    now = list_entry(list_next(&(now->elem)), struct frame, elem);
  }
  return NULL;
}
/*
    struct list_elem* e;

    if (list_size(&frame_table) == 1)
    {
        struct frame* temp = list_entry(list_begin(&frame_table), struct frame, elem);
        if (temp -> memory == NULL)
        {
            temp -> memory = palloc_get_page (flags);
            return temp;
        }
        else
        {
            return NULL;
        }
    }
*/
    int i = 0;
    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(&frame_table))
    {
        i++;
        struct frame* temp = list_entry(e, struct frame, elem);
        if (temp -> memory == NULL)
        {
            temp -> memory = palloc_get_page (flags);
            return temp;
        }
    }
    return NULL;
}

void free_frame(void* memory)
{
  struct list_elem* e;

  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(&frame_table))
  {
    struct frame* frame = list_entry(e, struct frame, elem);
    if (frame -> memory == memory)
    {
      palloc_free_page (memory);
      frame -> memory = NULL;
      frame -> tid = -1;
      break;
    }
  }
}

struct frame *
number_to_frame (tid_t finding_no)
{
  if (list_empty(&frame_table))
    return NULL;
  struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);
  while (now != NULL)
  {
    if (now -> frame_number == finding_no)
      return now;
    if (now == list_entry(list_end(&frame_table), struct frame, elem))
      break;
    now = list_entry(list_next(&(now->elem)), struct frame, elem);
  }
  return NULL;
}
