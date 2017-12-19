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
    //block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[109];               /* Not used. */
    uint32_t direct_index;
    uint32_t indirect_index;
    uint32_t double_indirect_index;
    block_sector_t block_ptr[14];
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
    //struct inode_disk data;             /* Inode content. */

    off_t length;                       /* File size in bytes. */
    off_t read_length;
    uint32_t direct_index;
    uint32_t indirect_index;
    uint32_t double_indirect_index;
    block_sector_t block_ptr[14];
  };

off_t
inode_grow (struct inode *myinode, off_t length)
{
  static char voids[512];

  if (bytes_to_sectors(length) == bytes_to_sectors(myinode->length))
  {
    return length;
  }

  size_t grow_sectors = bytes_to_sectors(length) - bytes_to_sectors(myinode->length);

  while (myinode->direct_index < 4 && grow_sectors != 0)
  {
    free_map_allocate (1, &myinode->block_ptr[myinode->direct_index]);
    block_write(fs_device, myinode->block_ptr[myinode->direct_index], voids);
    myinode->direct_index++;
    grow_sectors--;
  }

  while (myinode->direct_index < 13 && grow_sectors != 0)
  {
    block_sector_t blocks[128];
    if (myinode->indirect_index == 0)
      free_map_allocate(1, &myinode->block_ptr[myinode->direct_index]);
    else
      block_read(fs_device, myinode->block_ptr[myinode->direct_index], &blocks);
    while (myinode->indirect_index < 128 && grow_sectors != 0)
    {
      free_map_allocate(1, &blocks[myinode->indirect_index]);
      block_write(fs_device, blocks[myinode->indirect_index], voids);
      myinode->indirect_index++;
      grow_sectors--;
    }
    block_write(fs_device, myinode->block_ptr[myinode->direct_index], &blocks);
    if (myinode->indirect_index == 128)
    {
      myinode->indirect_index = 0;
      myinode->direct_index++;
    }
  }
  if (myinode->direct_index == 13 && grow_sectors != 0)
  {
    block_sector_t level_1[128];
    block_sector_t level_2[128];

    if (myinode->double_indirect_index == 0)
      free_map_allocate(1, &myinode->block_ptr[myinode->direct_index]);
    else
      block_read(fs_device, myinode->block_ptr[myinode->direct_index], &level_1);

    while (myinode->indirect_index < 14 && grow_sectors != 0)
    {
      if (myinode->double_indirect_index == 0)
        free_map_allocate(1, &level_1[myinode->indirect_index]);
     else
        block_read(fs_device, level_1[myinode->indirect_index], &level_2);
      while (myinode->double_indirect_index < 14 && grow_sectors != 0)
      {
        free_map_allocate(1, &level_2[myinode->double_indirect_index]);
        block_write(fs_device, level_2[myinode->double_indirect_index], voids);
        myinode->double_indirect_index++;
        grow_sectors--;
      }

    block_write(fs_device, level_1[myinode->indirect_index], &level_2);

      if (myinode->double_indirect_index == 128)
      {
        myinode->double_indirect_index = 0;
        myinode->indirect_index++;
      }
    }
    block_write(fs_device, myinode->block_ptr[myinode->direct_index], &level_1);
  }
  return length;
}


void
inode_free (struct inode *myinode)
{
  size_t num_sector = bytes_to_sectors(myinode->length);
  size_t index = 0;

  if(num_sector == 0)
  {
    return;
  }

  while (index < 4 && num_sector != 0)
  {
    free_map_release (myinode->block_ptr[index], 1);
    num_sector--;
    index++;
  }

  while (index < 13 && num_sector != 0)
  {
    size_t free_blocks = num_sector < 128 ?  num_sector : 128;

    int i;
    block_sector_t block[128];
    block_read(fs_device, myinode->block_ptr[index], &block);

    for (i = 0; i < free_blocks; i++)
    {
      free_map_release(block[i], 1);
      num_sector--;
    }

    free_map_release(myinode->block_ptr[index], 1);
    index++;
  }

  if (myinode->direct_index == 13)
  {
    size_t i, j;
    block_sector_t level_1[128], level_2[128];

    block_read(fs_device, myinode->block_ptr[13], &level_1);

    size_t indirect_blocks = DIV_ROUND_UP(num_sector, 128 * 512);

    for (i = 0; i < indirect_blocks; i++)
    {
      size_t free_blocks = num_sector < 128 ? num_sector : 128;

      block_read(fs_device, level_1[i], &level_2);

      for (j = 0; j < free_blocks; j++)
      {
        free_map_release(level_2[j], 1);
        num_sector--;
      }

      free_map_release(level_1[i], 1);
    }

    free_map_release(myinode->block_ptr[13], 1);
  }
}


bool inode_alloc (struct inode_disk * myinode_disk)
{
  struct inode myinode;
  myinode.length = 0;
  myinode.direct_index = 0;
  myinode.indirect_index = 0;
  myinode.double_indirect_index = 0;
  inode_grow(&myinode, myinode_disk->length);
  myinode_disk->direct_index = myinode.direct_index;
  myinode_disk->indirect_index = myinode.indirect_index;
  myinode_disk->double_indirect_index = myinode.double_indirect_index;
  memcpy(&myinode_disk->block_ptr, &myinode.block_ptr, 14 * sizeof(block_sector_t));
  return true;
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t length, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos < length)
  {
    uint32_t blocks[128];
    if (pos < 4 * 512)
    {
      return inode->block_ptr[pos / 512];
    }
    // indirect blocks
    else if (pos < 1156 * 512)
    {
      // read up corresponding indirect block
      pos -= 2048;
      block_read(fs_device, inode->block_ptr[pos / (128 * 512) + 4], &blocks);
      return blocks[pos%(128 * 512) / 512];
    }
    else
    {
      block_read(fs_device, inode->block_ptr[13], &blocks);
      pos -= (4 + 9 * 128) * 512;
      block_read(fs_device, blocks[pos / (128 * 512)], &blocks);
      return blocks[(pos%(128*512))/512];
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
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    if (inode_alloc(disk_inode))
    {
      block_write (fs_device, sector, disk_inode);
      success = true;
    }
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
  inode->length = my_inode_disk.length;
  inode->read_length = my_inode_disk.length;
  inode->direct_index = my_inode_disk.direct_index;
  inode->indirect_index = my_inode_disk.indirect_index;
  inode->double_indirect_index = my_inode_disk.double_indirect_index;
  memcpy(&inode->block_ptr, &my_inode_disk.block_ptr, 14 * sizeof(block_sector_t));
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
          //free_map_release (inode->data.start,
          //                  bytes_to_sectors (inode->data.length));
          inode_free(inode);
        }
      else
        {
          my_inode_disk.length = inode -> length;
          my_inode_disk.magic = INODE_MAGIC;
          my_inode_disk.direct_index = inode -> direct_index;
          my_inode_disk.indirect_index = inode -> indirect_index;
          my_inode_disk.double_indirect_index = inode -> double_indirect_index;
          memcpy(&my_inode_disk.block_ptr, &inode -> block_ptr, 14 * sizeof(block_sector_t));
          block_write(fs_device, inode -> sector, &my_inode_disk);
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
  off_t length = inode->read_length;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, inode->read_length, offset);
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
    inode->length = inode_grow(inode, offset + size);
  }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, inode->length, offset);
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
  return inode->length;
}
