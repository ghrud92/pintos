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

void* get_free_frame()
{
  init_table();

  void* memory = palloc_get_page(PAL_USER);

  struct frame* frame = malloc (sizeof(struct frame));
  frame -> memory = memory;
  frame -> tid = thread_tid();
  list_push_back (&frame_table, &frame->elem);

  return memory;
}
