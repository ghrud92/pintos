#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

static struct list opfilelist;

bool create (const char * file, unsigned initial_size)
{
  if (file == NULL)
    return false;
  else
  {
    filesys_create (*file, *initial_size);
    return true;
  }
} 

int open (const char * file)
{
  static int nextfd = 2;
  struct openedfile * opfile = new openedfile;
  if (file != NULL)
  {
    opfile -> file = filesys_open (*file);
    opfile -> fd = nextfd;
    list_push_front (&opfilelist, &(opfile -> opelem));
    fd++;
    return opfile -> fd;
  }
  else
    return -1;
}

void close (int fd)
{
  if(!list_empty(&opfilelist))
  {
    struct thread * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
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
  power_off();
} 

void exit (int status)
{

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
      create(ptr+4, ptr+5);
      break;
    case SYS_OPEN
      f -> eax = open (ptr+1);
      break;
    case SYS_CLOSE
      close (ptr+1);
  }
  thread_exit ();
}
