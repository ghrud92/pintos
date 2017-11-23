#include "vm/page.h"
#include "vm/frame.h"

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
    if (file_read (page -> file, kpage, page -> page_read_bytes) != (int) page_raed_bytes)
    {
        free_frame(kpage);
        return false;
    }
    memset (kpage + page -> page_read_bytes, 0, page -> page_zero_bytes);

    // add the page to the process's address space
    if (!install_page (upage, kpage, writable))
    {
        free_frame (kpage);
        return false;
    }

    return true;
}
