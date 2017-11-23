#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

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
    uint8_t* kpage = get_free_frame(PAL_USER);
    if (kpage == NULL)
        return false;

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

struct page* get_page(uint8_t* upage)
{
    struct page temp;
    temp.upage = pg_round_down(upage);

    struct list_elem* e;
    struct thread* t = thread_current();
    for (e = list_begin(&t -> page_table);
        e != list_end(&t -> page_table);
        e = list_next(&t -> page_table))
    {
        struct page* current = list_entry(e, struct page, elem);
        if (current -> upage == temp.upage)
        {
            return current;
        }
    }
    return NULL;
}
