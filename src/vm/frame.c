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
    struct list_elem* e;

    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(&frame_table))
    {
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
