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
#include "threads/synch.h"
#include "vm/page.h"


static void syscall_handler (struct intr_frame *);

static struct list opfilelist;
struct lock file_lock;

void address_check (void * addr)
{
  // return true;
  //if (!is_user_vaddr(addr) || !pagedir_get_page(thread_current()->pagedir, addr))
  if (!is_user_vaddr(addr))
    exit(-1);
  struct page * mypage = get_page((void *) addr);
  if (mypage)
  {
    if(!load_page(mypage))
      exit(-1);
  }
  //about grow stack
  else
	  return;
}

bool create (const char * file, unsigned initial_size)
{
  // file name is NULL
  if (file == NULL)
  {
    exit(-1);
    return false;
  }

  // check whether the file pointer is valid
  address_check(file);

  // the length of file name is 0
  if (file[0] == '\0')
  {
    exit(-1);
    return false;
  }

  // check the length of name
  char *temp = file;
  int i = 0;
  while(*temp != '\0')
  {
    i++;
    temp++;
    if (i > 14)
    {
      return false;
    }
  }

  // check the file is already exist
  int fd = open (file);
  if (fd != -1)
  {
    close (fd);
    return false;
  }

  bool success;
  lock_acquire(&file_lock);
  success = filesys_create (file, initial_size);
  lock_release (&file_lock);
  return success;
}

int open (const char * file)
{
  // check whether the file pointer is valid
  address_check(file);

  static int nextfd = 2;
  struct openedfile * opfile = malloc (sizeof(struct openedfile) * 1);
  if (file != NULL)
  {
    lock_acquire(&file_lock);
    opfile -> file = filesys_open (file);
    if (opfile -> file == NULL)
    {
      lock_release(&file_lock);
      return -1;
    }
    // printf("%s %d\n", "file open", nextfd);
    opfile -> fd = nextfd;
    opfile -> caller = thread_current();
    list_push_front (&opfilelist, &(opfile -> opelem));
    nextfd++;
    lock_release(&file_lock);
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
        if (now -> caller -> tid == thread_current() -> tid)
        {
          lock_acquire(&file_lock);
          file_close (now -> file);
          lock_release(&file_lock);
          list_remove(&(now->opelem));
          free(now);
          return;
        }
        else
        {
          exit(-1);
          return;
        }
      }
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next == NULL);
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
        lock_acquire(&file_lock);
        file_close (now -> file);
        lock_release(&file_lock);
        list_remove(&(now->opelem));
        free(now);
        break;
      }
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next != NULL);
  }
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_current()->exit_status = status;
  thread_exit();
}


int write (int fd , const void * buffer , unsigned size )
{
  // check whether the buffer address is valid or not
  // address_check(&buffer);
  address_check(buffer);

  // stdin
  if (fd == 0)
  {
    exit(-1);
  }

  if (fd == 1)
  {
    putbuf (buffer,size);
    return size;
  }
  else
  {
    // if there is no opened file
    if (list_empty(&opfilelist))
    {
      return -1;
    }

    lock_acquire(&file_lock);
    int byte = 0;
    struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
    do
    {
      if ((now -> fd) == fd)
      {
        byte = file_write (now -> file, buffer, size);
        break;
      }
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next != NULL);
    lock_release(&file_lock);

    return byte;
  }
}

void get_args (struct intr_frame * f, int * arg, int num_args)
{
  address_check(f->esp);
  int * ptr;
  int i;
  for (i = 0; i < num_args; i++)
    {
      ptr = (int *) f->esp + i + 1;
      address_check((void *) ptr);
      arg[i] = * ptr;
    }
}

int filesize (int fd)
{
  int size = 0;
  struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
  lock_acquire(&file_lock);
  do
  {
    if ((now -> fd) == fd)
    {
      size = file_length (now -> file);
      break;
    }
    now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
  } while(now->opelem.next != NULL);
  lock_release (&file_lock);

  return size;
}

unsigned tell (int fd)
{
  off_t offset = 0;
  struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
  do
  {
    if ((now -> fd) == fd)
    {
      lock_acquire(&file_lock);
      offset = file_tell (now -> file);
      lock_release(&file_lock);
      break;
    }
    now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
  } while(now->opelem.next != NULL);

  return offset;
}

void seek (int fd, unsigned position)
{
  struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
  do
  {
    if ((now -> fd) == fd)
    {
      lock_acquire(&file_lock);
      file_seek (now -> file, position);
      lock_release(&file_lock);
    }
    now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
  } while(now->opelem.next != NULL);
}

int read (int fd, const void *buffer, unsigned size)
{
  // check whether the buffer address is valid or not
  address_check(buffer);

  // stdout fd
  if (fd == 1)
  {
    exit(-1);
  }

  // stdin
  if (fd == 0)
  {
    unsigned i;
    int * temp_buffer = buffer;
    for (i = 0; i < size; i++)
    {
      temp_buffer[i] = input_getc();
      return size;
    }
  }
  else
  {
    // if there is no opened file
    if (list_empty(&opfilelist))
    {
      return -1;
    }

    int byte = -1;
    lock_acquire(&file_lock);
    struct openedfile * now = list_entry(list_front(&opfilelist), struct openedfile, opelem);
    do
    {
      if ((now -> fd) == fd)
      {
        byte = file_read (now -> file, buffer, size);
        break;
      }
      now = list_entry(list_next(&(now->opelem)), struct openedfile, opelem);
    } while(now->opelem.next != NULL);
    lock_release(&file_lock);

    return byte;
  }
}

bool remove (const char *file)
{
  bool success;
  lock_acquire(&file_lock);
  success = filesys_remove (file);
  lock_release(&file_lock);
  return success;
}

int exec (const char *cmd_line)
{
  // change the address from kernel to user memory
  void *user_memory = pagedir_get_page(thread_current()->pagedir, cmd_line);
  if (!user_memory)
  {
    exit(-1);
  }
  // lock_acquire(&file_lock);
    int result = process_execute (user_memory);
  // lock_release(&file_lock);
  return result;
}

int wait (int pid)
{
  return process_wait (pid);
}

void
syscall_init (void)
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init (&opfilelist);
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int args[100];
  address_check (f->esp);
  int * ptr = f -> esp;
  switch (*ptr) {
    case SYS_HALT:
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
    case SYS_FILESIZE:
      get_args(f, &args[0], 1);
      f -> eax = filesize((int) args[0]);
      break;
    case SYS_SEEK:
      get_args(f, &args[0], 2);
      seek((int) args[0], (unsigned) args[1]);
      break;
    case SYS_TELL:
      get_args(f, &args[0], 1);
      tell((int) args[0]);
      break;
    case SYS_READ:
      get_args(f, &args[0], 3);
      f -> eax = read ((int) args[0], (void *) args[1], (unsigned) args[2]);
      break;
    case SYS_REMOVE:
      get_args(f, &args[0], 1);
      f -> eax = remove ((char *) args[0]);
      break;
    case SYS_EXEC:
      get_args(f, &args[0], 1);
      f -> eax = exec ((char *) args[0]);
      break;
    case SYS_WAIT:
      get_args(f, &args[0], 1);
      f -> eax = wait ((int) args[0]);
      break;
  }
}
