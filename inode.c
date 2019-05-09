#include "lfs.h"
#include "dir.h"
#include "inode.h"

int init_inode(int block_id, int max_inode)
{
	struct inode_page *temp_inode_page = malloc(INODE_PAGE_SIZE);
	if (!temp_inode_page)
	{
		return -ENOMEM;
	}
  int pages =(int) ceil(max_inode/7);

  int last_page = block_id+pages;
  for (int i = block_id; i < last_page+1; i++) {

    if (i = last_page)
    {
        temp_inode_page->next_page = 0;
    }
    else
    {
      temp_inode_page->next_page = i;
    }

  	temp_inode_page->free_ids = 7;
  	// printf("init inode : about to write inode\n");
  	if (writeblock(temp_inode_page, block_id, INODE_PAGE_SIZE) < 0)
  	{
  		return -ENOMEM;
  	}
  }

	free(temp_inode_page);
	return pages;
}

ino_t get_inode_id()
{
  struct volume_control *table = readblock(0, VOLUME_CONTROL_SIZE);
	ino_t lowest_inode_id = 1;
  struct inode_page *temp_inode_page = readblock(table->inode_block, sizeof(struct inode_page));

  while(temp_inode_page->free_ids == 0){ // if empty, look for next table
		if (temp_inode_page->next_page == 0){
			free(temp_inode_page);
			return -ENOMEM;
		}
		else
		{
			lowest_inode_id += 5;
      temp_inode_page = readblock(temp_inode_page->next_page, INODE_PAGE_SIZE);
		}
	}

	struct inode inode_check;
	for (size_t i = 0; i < 5; i++) {
		inode_check = temp_inode_page->inodes[i];
		if (inode_check.inode_no == lowest_inode_id)
		{
			lowest_inode_id++;
		}
	}
	lowest_inode_id++;
	free(temp_inode_page);
  printf("returning inode %d\n", lowest_inode_id);
	return lowest_inode_id;
}

/*
	id = 27
	inode_ids p1 : 1  2  3  4  5  6  7  8  9  10 | next_page = p2
	inode_ids p2 : 11 12 13 14 15 16 17 18 19 20 | next_page = p3
	inode_ids p3 : 21 22 23 24 25 26 27 28 29 30 | next_page = NULL
*/

int get_inode_page(struct inode_page *buffer, ino_t id)
// Gets the page that contains the inode
{
	struct volume_control *table = get_volume_control();
  if (table) {
    return -EFAULT;
  }

  struct inode_page *temp_inode_page = readblock(table->inode_block, INODE_PAGE_SIZE);
	if(temp_inode_page < 0)
	{
		free(table);
		return -EFAULT;
	}

	ino_t max_page_inode = 7;
	while (id > max_page_inode)
	{
    struct inode_page *temp_inode_page =
                                  readblock(table->inode_block, INODE_PAGE_SIZE);
		if (temp_inode_page < 0)
		{
      free(table);
			return -EFAULT;
		}
		id = id-5; // go to next page, decrease id
	}
	memcpy(buffer, temp_inode_page, INODE_PAGE_SIZE);
	free(table);
	free(temp_inode_page);
	return 0;
}

int delete_inode(ino_t id)
{
	struct inode_page *temp_inode_page = malloc(INODE_PAGE_SIZE);
	if (!temp_inode_page)
	{
		return -ENOMEM;
	}
	if (get_inode_page(temp_inode_page, id) < 0)
	{
			free(temp_inode_page);
			return -EAGAIN;
	}
	ino_t inode_page_id = id % 7;

	// clear inode array index
	memset(&temp_inode_page->inodes[inode_page_id], 0, INODE_PAGE_SIZE);

  temp_inode_page->free_ids++;
	free(temp_inode_page);
	return 0;
}
