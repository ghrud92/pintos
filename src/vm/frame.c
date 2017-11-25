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

int get_free_frame_number(enum palloc_flags flags)
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

  return frame -> frame_number;
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
      return now;
    }

    if (now -> elem.next != NULL)
        now = list_entry(list_next(&(now->elem)), struct frame, elem);
    else
        break;
  }
  return NULL;
}

void free_frame(void* memory)
{
    struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);
    while (now != NULL)
    {
        if (now -> memory == memory)
        {
            palloc_free_page (memory);
            now -> memory = NULL;
            now -> tid = -1;
            break;
        }

        if (now -> elem.next != NULL)
            now = list_entry(list_next(&(now->elem)), struct frame, elem);
        else
            break;
    }
}

struct frame *
number_to_frame (tid_t finding_no)
{
    printf("%s %d\n", "global number ", frame_number);
  if (list_empty(&frame_table))
    return NULL;
  struct frame * now = list_entry(list_front(&frame_table), struct frame, elem);
  if (is_kernel_vaddr (now)){
      printf("%s\n", "aa");
  }
  // printf("%s %d\n", "tid ", now -> tid);
  return now;
  // while (now != NULL)
  // {
  //     printf("%s %d\n", "loop frame number", now -> frame_number);
  //     printf("%s\n", "b");
  //   // if (now -> frame_number == finding_no)
  //   // {
  //       printf("%s\n", "c");
  //     return now;
  //   // }
  //     printf("%s\n", "d");
  //   if (now == list_entry(list_end(&frame_table), struct frame, elem))
  //     break;
  //   now = list_entry(list_next(&(now->elem)), struct frame, elem);
  // }
  return NULL;
}
