#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include <list.h>

static void syscall_handler (struct intr_frame *);

static struct list opfilelist;

bool create (const char * file, unsigned initial_size)
{
  if (file == NULL)
    return false;
  else
  {
    filesys_create (file, initial_size);
    return true;
  }
}

int open (const char * file)
{
  static int nextfd = 2;
  struct openedfile *opfile = (openedfile*) malloc (sizeof(openedfile) * 1);
  if (file != NULL)
  {
    opfile -> file = filesys_open (*file);
    opfile -> fd = nextfd;
    opfile -> caller = thread_current();
    list_push_front (&opfilelist, &(opfile -> opelem));
    nextfd++;
    return opfile -> fd;
  }
  else
    return -1;
}

void close (int fd)
{
  if(!list_empty(&opfilelist))
  {
    struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
    do
    {
      if (now -> fd == fd)
      {
        file_close (now -> file);
        list_remove(&(now->opelem));
        return;
      }
      now = list_entry(list_next(&(now->elem)), struct openedfile, opelem);
    } while(now->elem.next != NULL);
  }
  return;
}

void halt ()
{
  shutdown_power_off();
}

void exit (int status)
{
  struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
  do
  {
    if (now -> caller == thread_current())
    {
      file_close (now -> file);
      list_remove(&(now->opelem));
    }
    now = list_entry(list_next(&(now->elem)), struct openedfile, opelem);
  } while(now->elem.next != NULL);
  thread_current()->exit_status = status;
  thread_exit();
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init (&opfilelist);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int * ptr = f -> esp;
  int i;
  struct thread * cur = thread_current();
  switch (*ptr) {
    case SYS_HALT:
      halt();
      break;
    case SYS_CREATE
      if(!is_user_vaddr(*(ptr+4)) || !is_user_vaddr(ptr+5))
        exit(-1);
      create(ptr+4, *(ptr+5));
      break;
    case SYS_OPEN
      if(!is_user_vaddr(*(ptr+1)))
        exit(-1);
      f -> eax = open (ptr+1);
      break;
    case SYS_CLOSE
      if(!is_user_vaddr(ptr+1))
        exit(-1);
      close (*(ptr+1));
      break;
    case SYS_EXIT
      if(!is_user_vaddr(ptr+1))
        exit(-1);
      exit(*(ptr+1));
      break;
  }
}
