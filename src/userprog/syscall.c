#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
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
  struct openedfile * opfile = malloc (sizeof(struct openedfile) * 1);
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
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next != NULL);
  }
  return;
}

void halt ()
{
  shutdown_power_off();
}

void exit (int status)
{
  if(!list_empty(&opfilelist))
  {
    struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
    do
    {
      if (now -> caller == thread_current())
      {
        file_close (now -> file);
        list_remove(&(now->opelem));
      }
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next != NULL);
  }
  thread_current()->exit_status = status;
  thread_exit();
}

int write (int fd , const void * buffer , unsigned size )
{
  printf("%s\n", "write");
  printf("%s %d\n", "fd is", fd);
  if (fd == 1)
  {
    putbuf (buffer,size);
    return size;
  }
  else
  {
    printf("%s\n", "fd is not 1");
    struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
    do
    {
      if ((now -> fd) == fd)
      {
        return file_write (now -> file, buffer, size);
      }
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next != NULL);
    return 0;
  }
}

void address_check (void * addr)
{
  // return true;
  if (!is_user_vaddr(addr) || !pagedir_get_page(thread_current()->pagedir, addr))
    exit(-1);
  else
	  return;
}

void get_args (struct intr_frame * f, int * arg, int num_args)
{
  int * ptr;
  for (int i = 0; i < num_args; i++)
    {
      ptr = (int *) f->esp + i + 1;
      address_check((void *) ptr);
      arg[i] = * ptr;
    }
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
  int args[100];
  int * ptr = f -> esp;
  switch (*ptr) {
    case SYS_HALT:
      printf("%s\n", "halt");
      halt();
      break;
    case SYS_CREATE:
      get_args(f, &args[0], 2);
      f -> eax = create((char *) args[0], (unsigned) args[1]);
      break;
    case SYS_OPEN:
      get_args(f, &args[0], 1);
      f -> eax = open((char *) args[0]);
      break;
    case SYS_CLOSE:
      get_args(f, &args[0], 1);
      close ((int) args[0]);
      break;
    case SYS_EXIT:
      get_args(f, &args[0], 1);
      exit((int) args[0]);
      break;
    case SYS_WRITE:
      get_args(f, &args[0], 3);
      f -> eax = write((int) args[0], (void *) args[1], (unsigned) args[2]);
      break;
  }
}
