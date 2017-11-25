#include <stdio.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include <list.h>
#include <string.h>
#include "filesys/file.h"

void init_page_table(struct list* page_table)
{
    list_init(page_table);
}

void destroy_page_table(struct list* page_table)
{
    struct list_elem* e;

}

bool load_page(struct page* page)
{
    if(!page)
        return false;
    int frame_number = get_free_frame_number(PAL_USER, page);
    uint8_t* kpage = number_to_frame(frame_number);
    // uint8_t* kpage = palloc_get_page (PAL_USER);
    if (kpage == NULL)
        return false;

    page -> frame_number = frame_number;

    // load this page
    if (file_read (page -> file, kpage, page -> read_bytes) != (int) page -> read_bytes)
    {
        free_frame(kpage);
        return false;
    }
    memset (kpage + page -> read_bytes, 0, page -> zero_bytes);

    // add the page to the process's address space
    if (!install_page (page -> upage, kpage, page -> writable))
    {
        free_frame (kpage);
        return false;
    }

    return true;
}

struct page* find_page(void* upage)
{
    struct page temp;
    temp.upage = pg_round_down(upage);

    struct thread* t = thread_current();

    if (list_empty(&t -> page_table))
        return NULL;

    struct page * now = list_entry(list_front(&t -> page_table), struct page, elem);
    while (now != NULL)
    {
      if (now -> upage == temp.upage)
      {
        return now;
      }

      if (now -> elem.next != NULL)
        now = list_entry(list_next(&(now->elem)), struct page, elem);
      else
        break;
    }

    return NULL;
}

bool grow_stack (void * ptr)
{
    struct page * expage = malloc(sizeof(struct page));
    if (!expage)
        return false;
    expage -> upage = pg_round_down(ptr);
    expage -> writable = true;
    expage -> frame_number = get_free_frame_number(PAL_USER, expage -> upage);
    if(expage -> frame_number == -1)
    {
        free(expage);
        return false;
    }

    void* exframe = number_to_frame (expage -> frame_number);
    if (!install_page(expage -> upage, exframe, expage -> writable))
    {
        free(expage);
        free_frame (exframe);
        return false;
    }
    list_push_front (&(thread_current()->page_table), &expage->elem);
    return true;
}
