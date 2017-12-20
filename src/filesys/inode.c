#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[108];               /* Not used. */
    uint32_t direct_index;
    uint32_t indirect_index;
    block_sector_t inode_blocks[16];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

off_t
inode_grow (struct inode *myinode, off_t length)
{
  static char voids[BLOCK_SECTOR_SIZE];

  size_t grow_remain = bytes_to_sectors(length) - bytes_to_sectors(myinode->data.length);
  if (!grow_remain)
    return length;

  while (myinode->data.direct_index < 4 && grow_remain != 0)
  {
    free_map_allocate (1, &myinode->data.inode_blocks[myinode->data.direct_index]);
    block_write(fs_device, myinode->data.inode_blocks[myinode->data.direct_index], voids);
    myinode->data.direct_index++;
    grow_remain--;
  }

  while (myinode->data.direct_index < 13 && grow_remain != 0)
  {
    block_sector_t blocks[BLOCK_SECTOR_SIZE];
    if (myinode->data.indirect_index == 0)
      free_map_allocate(1, &myinode->data.inode_blocks[myinode->data.direct_index]);
    else
      block_read(fs_device, myinode->data.inode_blocks[myinode->data.direct_index], &blocks);
    while (myinode->data.indirect_index < 128 && grow_remain != 0)
    {
      free_map_allocate(1, &blocks[myinode->data.indirect_index]);
      block_write(fs_device, blocks[myinode->data.indirect_index], voids);
      myinode->data.indirect_index++;
      grow_remain--;
    }
    block_write(fs_device, myinode->data.inode_blocks[myinode->data.direct_index], &blocks);
    if (myinode->data.indirect_index == 128)
    {
      myinode->data.indirect_index = 0;
      myinode->data.direct_index++;
    }
  }

  return length;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
    uint32_t blocks[128];
    if (pos < 4 * BLOCK_SECTOR_SIZE)
    {
      return inode->data.inode_blocks[pos / BLOCK_SECTOR_SIZE];
    }
    else
    {
      pos -= 4 * BLOCK_SECTOR_SIZE;
      block_read(fs_device, inode->data.inode_blocks[pos / (128 * BLOCK_SECTOR_SIZE) + 4], &blocks);
      return blocks[(pos%(128 * BLOCK_SECTOR_SIZE)) / BLOCK_SECTOR_SIZE];
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    struct inode *myinode;
    myinode = calloc (1, sizeof *myinode);
    myinode->data.length = 0;
    myinode->data.direct_index = 0;
    myinode->data.indirect_index = 0;
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    inode_grow(myinode, disk_inode->length);
    disk_inode->direct_index = myinode->data.direct_index;
    disk_inode->indirect_index = myinode->data.indirect_index;
    memcpy(&disk_inode->inode_blocks, &myinode->data.inode_blocks, 16 * sizeof(block_sector_t));
    block_write (fs_device, sector, disk_inode);
    success = true;
    free (disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
  {
    inode = list_entry (e, struct inode, elem);
    if (inode->sector == sector)
    {
      inode_reopen (inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  struct inode_disk my_inode_disk;

  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &my_inode_disk);
  inode->data.length = my_inode_disk.length;
  inode->data.direct_index = my_inode_disk.direct_index;
  inode->data.indirect_index = my_inode_disk.indirect_index;
  memcpy(&inode->data.inode_blocks, &my_inode_disk.inode_blocks, 16 * sizeof(block_sector_t));
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  struct inode_disk my_inode_disk;
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      free_map_release (inode->sector, 1);
      size_t num_sector = bytes_to_sectors(inode->data.length);
      int i = 0;
      if(num_sector == 0)
        return;
      while (i < 4 && num_sector != 0)
      {
        free_map_release (inode->data.inode_blocks[i], 1);
        num_sector--;
        i++;
      }
      while (i < 13 && num_sector != 0)
      {
        size_t num_free;
        if (num_sector < 128)
          num_free = num_sector;
        else
          num_free = 128;
        int j = 0;
        block_sector_t remove_block[BLOCK_SECTOR_SIZE];
        block_read(fs_device, inode->data.inode_blocks[i], &remove_block);
        while (j < num_free)
        {
          free_map_release(remove_block[j], 1);
          j++;
          num_sector--;
        }
        free_map_release(inode->data.inode_blocks[i], 1);
        i++;
      }
    }
    else
    {
      my_inode_disk.length = inode->data.length;
      my_inode_disk.magic = INODE_MAGIC;
      my_inode_disk.direct_index = inode->data.direct_index;
      my_inode_disk.indirect_index = inode->data.indirect_index;
      memcpy(&my_inode_disk.inode_blocks, &inode->data.inode_blocks, 16 * sizeof(block_sector_t));
      block_write(fs_device, inode->sector, &my_inode_disk);
    }
    free (inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
  {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Read full sector directly into caller's buffer. */
      block_read (fs_device, sector_idx, buffer + bytes_read);
    }
    else
    {
      /* Read sector into bounce buffer, then partially copy
         into caller's buffer. */
      if (bounce == NULL)
      {
        bounce = malloc (BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }
      block_read (fs_device, sector_idx, bounce);
      memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }
  free (bounce);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if(offset + size > inode_length(inode))
  {
    inode->data.length = inode_grow(inode, offset + size);
  }

  while (size > 0)
  {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Write full sector directly to disk. */
      block_write (fs_device, sector_idx, buffer + bytes_written);
    }
    else
    {
      /* We need a bounce buffer. */
      if (bounce == NULL)
      {
        bounce = malloc (BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }

      /* If the sector contains data before or after the chunk
         we're writing, then we need to read in the sector
         first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
        block_read (fs_device, sector_idx, bounce);
      else
        memset (bounce, 0, BLOCK_SECTOR_SIZE);
      memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      block_write (fs_device, sector_idx, bounce);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  free (bounce);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
