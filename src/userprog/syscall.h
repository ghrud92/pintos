#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

struct openedfile
{
    int fd;
    struct file * file;
    struct list_elem opelem;
    struct thread * caller;
};

#endif /* userprog/syscall.h */
