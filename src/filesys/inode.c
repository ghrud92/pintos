#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"


#include "threads/thread.h"
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/*
16384
inode create filesys_create �Ҷ� ���ִ� �κ�
inode write at // file_write �Ҷ� ���ִ� �κ�
inode read at // file_read �� �� �о� �ִ� �κ�

*/
/* On-disk inode.
Must be exactly DISK_SECTOR_SIZE bytes long. */
#ifdef P4_SUBDIR
#else	// P4_SUBDIR
#ifdef P4_FILE
struct inode_disk
{
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	disk_sector_t ind[MAX_SIZE_INDEX_D];
};
#else //P4_FILE
struct inode_disk
{
	disk_sector_t start;                /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	uint32_t unused[125];               /* Not used. */
	bool is_dir;
};
#endif //P4_FILE
#endif// P4_SUBDIR


/* Returns the number of sectors to allocate for an inode SIZE
bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}
#ifdef P4_SUBDIR
#else //P4_SUBDIR
#endif
/* In-memory inode. */




/* Returns the disk sector that contains byte offset POS within
INODE.
Returns -1 if INODE does not contain data for a byte at offset
POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
	ASSERT (inode != NULL);
#ifdef P4_FILE
	return DIV_ROUND_UP(pos, DISK_SECTOR_SIZE);
#else //P4_FILE
	if (pos < inode->data.length)
		return inode->data.start + pos / DISK_SECTOR_SIZE;
	else
		return -1;
#endif //P4_FILE
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
disk.
Returns true if successful.
Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool is_dir)
{
	struct inode_disk *disk_inode = NULL;
	bool success = false;
#ifdef P4_FILE
	uint8_t *bounce = NULL;
	size_t sectors_t;
	int i = 0;

#endif //P4_FILE 
	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	one sector in size, and you should fix that. */


	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL)
	{
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->parent_sec = 0;
		disk_inode->is_dir = is_dir;
		disk_inode->magic = INODE_MAGIC;

#ifdef P4_FILE

		sectors_t = sectors;
		bounce = malloc (DISK_SECTOR_SIZE);

		if(bounce == NULL)
		{
			return success;
		}

		memset (bounce, 0, DISK_SECTOR_SIZE);
		//use direct
		for(i = 0 ; i< (sectors > MAX_SIZE_INDEX_D ? MAX_SIZE_INDEX_D : sectors); i++)
		{
			if(free_map_allocate(1, &disk_inode->ind[i]) == false)
			{
				free(bounce);
				return success;
			}
			cache_write(filesys_disk, disk_inode->ind[i], bounce);
		}

		cache_write (filesys_disk, sector, disk_inode);
		free (disk_inode);
		success = true; 
#else //P4_FILE

		if (free_map_allocate (sectors, &disk_inode->start))
		{
#ifdef P4_CACHE
			cache_write (filesys_disk, sector, disk_inode);
#else//P4_CACHE
			disk_write (filesys_disk, sector, disk_inode);
#endif//P4_CACHE

			if (sectors > 0) 
			{
				static char init_zero[DISK_SECTOR_SIZE];
				size_t i;
				for (i = 0; i < sectors; i++) 
#ifdef P4_CACHE
			cache_write (filesys_disk, disk_inode->start + i, init_zero); 
#else//P4_CACHE
			disk_write (filesys_disk, disk_inode->start + i, init_zero); 
#endif//P4_CACHE

			}
			success = true; 
		} 
		free (disk_inode);

#endif //P4_FILE
	}

	return success;
}

/* Reads an inode from SECTOR
and returns a `struct inode' that contains it.
Returns a null pointer if memory allocation fails. */
struct inode *
	inode_open (disk_sector_t sector) 
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
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	
	inode->is_dir = false;
	inode->parent_sec = 0;
#ifdef P4_CACHE
	cache_read (filesys_disk, inode->sector, &inode->data);
#else //P4_CACHE
	disk_read (filesys_disk, inode->sector, &inode->data);
#endif //P4_CACHE
	
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
disk_sector_t
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

#ifdef P4_FILE
	size_t sectors;

	int i = 0;
#endif //P4_FILE
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
#ifdef P4_FILE
			
			sectors = bytes_to_sectors (inode->data.length);

			for(i = 0 ; i< (sectors > MAX_SIZE_INDEX_D ? MAX_SIZE_INDEX_D : sectors); i++)
			{
				free_map_release (inode->data.ind[i], 1);
				
			}

#else //P4_FILE
			free_map_release (inode->data.start,
				bytes_to_sectors (inode->data.length)); 
#endif //P4_FILE
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
	//printf("I'm in in the inode read at 1!\n");
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

#ifdef P4_FILE
	bounce = malloc (DISK_SECTOR_SIZE);
	if (bounce == NULL) return 0;
#endif //P4_FILE
	while (size > 0) 
	{
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_ind = byte_to_sector (inode, offset);
#ifdef P4_FILE
		if(offset % DISK_SECTOR_SIZE !=0)
		{
			sector_ind = sector_ind - 1;
		}
		sector_ind = get_sector(inode, sector_ind);
		////printf("I'm in in the inode read at sector_ind %d!\n", sector_ind);
#endif //P4_FILE
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;
#ifdef P4_FILE

		cache_read(filesys_disk, sector_ind, bounce);
		memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);





#else //P4_FILE

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
		{
			/* Read full sector directly into caller's buffer. */
#ifdef P4_CACHE
			cache_read(filesys_disk, sector_ind, buffer + bytes_read);
#else//P4_CACHE
			disk_read(filesys_disk, sector_ind, buffer + bytes_read); 
#endif//P4_CACHE
		}
		else 
		{
			/* Read sector into bounce buffer, then partially copy
			into caller's buffer. */
			if (bounce == NULL) 
			{
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
#ifdef P4_CACHE
			cache_read(filesys_disk, sector_ind, bounce);
			memcpy (buffer+bytes_read, bounce+sector_ofs, chunk_size);
#else//P4_CACHE
			disk_read (filesys_disk, sector_ind, bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
#endif//P4_CACHE
		}


#endif //P4_FILE







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
//	printf("I'm in in the inode wirte at star & length & offset %d %d %d!\n",inode->data.length, size, offset);
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;
#ifdef P4_FILE
	int i;
	int sectors_e;
	int sectors;
	char init_zero[DISK_SECTOR_SIZE];
#endif //P4_FILE
	if (inode->deny_write_cnt)
		return 0;


#ifdef P4_FILE
	bounce = malloc (DISK_SECTOR_SIZE);
	if (bounce == NULL) return 0;

	if(offset > inode->data.length)
	{	
		sectors = bytes_to_sectors (offset);

		memset(init_zero, 0, DISK_SECTOR_SIZE);
		for(i = 0 ; i< (sectors > MAX_SIZE_INDEX_D ? MAX_SIZE_INDEX_D : sectors); i++)
		{
			if(inode->data.ind[i] >= disk_size(filesys_disk)-100)
			{
				return -1;
			}
			free_map_allocate((size_t)1, &inode->data.ind[i]);
			cache_write(filesys_disk, inode->data.ind[i], init_zero);
		}
		inode->data.length = offset;
		cache_write(filesys_disk, inode->sector, inode);

//		printf("length[%d] offset[%d] sector[%d] size[%d]\n",inode->data.length, offset, sectors, size);
	}

#endif //P4_FILE
	//printf("I'm in in the inode wirte at 2!\n");
	while (size > 0) 
	{
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_ind = byte_to_sector (inode, offset);
		//printf("I'm in in the inode wirte at 3! %d\n", sector_ind);
#ifdef P4_FILE
		//printf("I'm in in the inode wirte at 3! %d\n", sector_ind);
		if (offset % DISK_SECTOR_SIZE != 0)
		{
			sector_ind--;
		}
		for (i = 0; i < MAX_SIZE_INDEX_D; i++)
		{
			if (inode->data.ind[i] == 0)
			{
				sectors_e = i;
				break;
			}
		}
		//�� �� ã��
		sector_ind = get_sector(inode, sector_ind);
//		printf("I'm in in the inode wirte at 4! %d\n", sector_ind);
#endif //P4_FILE
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
//		printf("I'm in in the inode wirte at 5!\n");
		////printf("I'm in in the inode wirte at inode_left! %d\n", inode_left);
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		////printf("I'm in in the inode wirte at sector_left! %d\n", sector_left);
		int min_left = inode_left < sector_left ? inode_left : sector_left;
		////printf("I'm in in the inode wirte at min_left! %d\n", min_left);
		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		//printf("I'm in in the inode wirte at chunk_size! %d\n", chunk_size);
#ifdef P4_FILE
#else //P4_FILE
		if (chunk_size <= 0)
			break;
#endif //P4_FILE
//		printf("I'm in in the inode wirte at 6\n");
		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
		{
//			printf("I'm in in the inode wirte at 7\n");
			/* Write full sector directly to disk. */
#ifdef P4_CACHE
			if(sector_ind >= disk_size(filesys_disk)-100)
			{
				return -1;
			}
			cache_write (filesys_disk, sector_ind, buffer + bytes_written);
#else //P4_CACHE
			disk_write (filesys_disk, sector_ind, buffer + bytes_written); 
#endif //P4_CACHE
		}

#ifdef P4_FILE
		else if (sector_ofs == 0 && chunk_size <= 0)
		{
	
//			printf("I'm in in the inode wirte at 8\n");
			
			memset(init_zero, 0, sizeof init_zero);
			free_map_allocate (1, &inode->data.ind[sectors_e]);
			if(inode->data.ind[sectors_e] >= disk_size(filesys_disk)-100)
			{
				return -1;
			}

			cache_write (filesys_disk, inode->data.ind[sectors_e], init_zero);

			inode->data.length += size < DISK_SECTOR_SIZE ? size : DISK_SECTOR_SIZE;
			chunk_size = size < DISK_SECTOR_SIZE ? size : DISK_SECTOR_SIZE;
			cache_write (filesys_disk, inode->sector, &inode->data);

			cache_write(filesys_disk, inode->data.ind[sectors_e], buffer + bytes_written);
		}
		else if (sector_ofs != 0 && chunk_size <= 0)
		{
//			printf("I'm in in the inode wirte at 9\n");
//			printf("I'm in in the inode wirte at 10 %d\n", chunk_size);
//			printf("I'm in in the inode wirte at 11 %d\n", sector_left);
			if (sector_left < size)
			{
				inode->data.length += sector_left;
				chunk_size = sector_left;
			}
			else
			{
				inode->data.length += size;
				chunk_size = size;
			}
			if(inode->sector >= disk_size(filesys_disk)-100)
			{
				return -1;
			}
			cache_write (filesys_disk, inode->sector, &inode->data);
			if (bounce == NULL)
			{
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
				{
					break;
				}
			}
			if(sectors_e == 0)
			{
				sectors_e=1;
			}
			cache_read (filesys_disk, inode->data.ind[sectors_e - 1], bounce);

			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			if(inode->data.ind[sectors_e - 1] >= disk_size(filesys_disk)-100)
			{
				return -1;
			}

			cache_write(filesys_disk, inode->data.ind[sectors_e - 1], bounce);
		}
#endif //P4_FILE


		else 
		{

			/* We need a bounce buffer. */
			if (bounce == NULL) 
			{

				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			we're writing, then we need to read in the sector
			first.  Otherwise we start with a sector of all init_zero. */
			if (sector_ofs > 0 || chunk_size < sector_left){
#ifdef P4_CACHE

				cache_read(filesys_disk, sector_ind, bounce);
#else//P4_CACHE
				disk_read (filesys_disk, sector_ind, bounce);
#endif//P4_CACHE
			}else{

				memset (bounce, 0, DISK_SECTOR_SIZE);
			}
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
#ifdef P4_CACHE
			if(sector_ind >= disk_size(filesys_disk)-100)
			{
				return -1;
			}
			cache_write (filesys_disk, sector_ind, bounce);
#else//P4_CACHE
			disk_write (filesys_disk, sector_ind, bounce); 
#endif//P4_CACHE
		}
	//	printf("I'm in in the inode wirte at 11 %d\n", chunk_size);
		/* Advance. */
		size -= chunk_size;

		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);
	////printf("I'm in in the inode wirte at 5! %d\n", bytes_written);
	//printf("bytes_written %d\n", bytes_written);
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
#ifdef P4_FILE
disk_sector_t get_sector(struct inode *inode, disk_sector_t sectors)
{
	disk_sector_t ret = 0;
//	struct single_list *s_list;
//	struct double_list *d_list;
//	int i = 0; 
//	int j = 0;

	if(sectors < MAX_SIZE_INDEX_D)
	{
		ret = inode->data.ind[sectors];
	}
	else
	{/*
		if(sectors < MAX_SIZE_INDEX_D + MAX_SIZE_INDEX)
		{
			sectors = sectors - MAX_SIZE_INDEX_D;
			s_list = calloc(1, sizeof(struct single_list));	
			if(s_list == NULL)
			{
				return ret;
			}
			cache_read (filesys_disk, inode->data.s_list, s_list);
			ret = s_list->ind[sectors];
			free (s_list);

		}
		else
		{
			sectors = sectors - MAX_SIZE_INDEX_D - MAX_SIZE_INDEX;

			d_list = calloc(1, sizeof(struct double_list));	
			if(d_list == NULL)
			{
				return ret;
			}
			cache_read (filesys_disk, inode->data.d_list, d_list);

			s_list = calloc(1, sizeof(struct single_list));	
			if(s_list == NULL)
			{
				free (d_list);
				return ret;
			}
			i = sectors / MAX_SIZE_INDEX;
			j = sectors - i * MAX_SIZE_INDEX;

			cache_read (filesys_disk, d_list->s_list[i], s_list);
			ret = s_list->ind[j];
			free (s_list);
			free (d_list);
		}*/
	}
	return ret;
}
#endif //P4_FILE
