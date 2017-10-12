#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>

void syscall_init (void);

struct openedfile
{
    int fd;
    struct file * file;
    struct list_elem opelem;
};

#endif /* userprog/syscall.h */
