#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"

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

  void* memory = palloc_get_page(flags);

  struct frame* frame = malloc (sizeof(struct frame));
  frame -> memory = memory;
  frame -> tid = thread_tid();
  list_push_back (&frame_table, &frame->elem);

  return memory;
}

void free_frame(void* memory)
{
  struct list_elem* e;

  for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(&frame_table))
  {
    struct frame* frame = list_entry(e, struct frame, elem);
    if (frame->memory == memory)
    {
      list_remove(e);
      free(frame);
      palloc_free_page (memory);
      break;
    }
  }
}
