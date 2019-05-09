#include "dir.h"
#include "lfs.h"

/*
*  /path/path1 path2/file
*/


int writeblock(void* buf, int block_id, size_t size)
// writes `size` bytes from `buf` to `block_id` file block
{
	// printf("writeblock : start\n");
	if (size < 0 || size > 512)
	{
		return -EINVAL;
	}
	if (buf == NULL)
	{
		return -EINVAL;
	}
	if (block_id < 0)
	{
		return -EINVAL;
	}
	// printf("writeblock : about to open disk\n");

	FILE* fp = fopen(DISKNAME, "r+");
  if(!fp)
  {
    return -EINVAL;
  }

	int offset = 512*block_id;
	// printf("writeblock : done opening disk\n");

	fseek(fp, offset, SEEK_SET);
	// printf("writeblock : About to fwrite\n");

	if(fwrite(buf, size, 1, fp) != 1){
		// printf("writeblock : fwrite failed\n");
		fclose(fp);
		return -EAGAIN;
	}
	// printf("writeblock : Done fwrite\n");
	fclose(fp);
	return size;
}

int readblock(void* buf, int block_id, size_t size)
// reads `size` bytes from disk into `buf`
{
	int res = 0;

	// printf("readblock : start. reading %zd bytes\n", size);
	if (size < 0 || size > 512)
	{
		return -EINVAL;
	}
  if (block_id < 0)
	{
		return -EINVAL;
	}
	// printf("readblock : done allocating\n");
	// printf("readblock : opening disk.\n");

	FILE* fp = fopen(DISKNAME, "r+");
  if(!fp)
  {
    return -EINVAL;
  }
	// printf("readblock : done opening\n");
	int offset = 512*block_id;
	// printf("readblock : before seek fp is %p\n", fp);
  fseek(fp, offset, SEEK_SET);
	// printf("readblock : after seek fp is %p\n", fp);

	// printf("readblock : about to fread\n");
	res = fread(buf, size, 1, fp);
  if(res != 1)//fseek positions the stream
  {
		fclose(fp);
		// printf("readblock : failed to fread. res was %d and size was %d\n", res, size);
    return -EAGAIN;
  }
	// printf("readblock : done with readblock\n");
	fclose(fp);
	return size;
}

int delete_block()
{
	return 0;
}

int init_inode(int block_id)
{
	struct inode_page *temp_inode_page = malloc(INODE_PAGE_SIZE);
	if (!temp_inode_page)
	{
		return -ENOMEM;
	}
	temp_inode_page->free_ids = 10;
	temp_inode_page->next_page = 0;
	// printf("init inode : about to write inode\n");
	if (writeblock(temp_inode_page, block_id, INODE_PAGE_SIZE) < 0)
	{
		free(temp_inode_page);
		return -ENOMEM;
	}
	// printf("init inode : done writing inode\n");

	free(temp_inode_page);
	return 0;
}

int init_volume(int nblocks, int nblock_size, int max_entries)
{
	// printf("Doing init volume.\n");
	struct volume_control *disk = malloc(sizeof(struct volume_control));
	if (!disk)
	{
		return -ENOMEM;
	}
	disk->blocks =nblocks;
	disk->block_size = nblock_size;
  disk->max_file_entries = max_entries;

	// printf("about to init inode.\n");
	init_inode(3);
	// printf("about to init head.\n");
	init_head(2, 1);
	// printf("Done init both.\n");

	// printf("\ntrying to read root attrs\n");
	struct linkedlist_dir *temp = malloc(LINKEDLIST_SIZE);
	readblock(temp, 2, LINKEDLIST_SIZE);
	// printf("Name is %s and size is %d\n", temp->name, temp->name_length);
	// printf("type is %d, next is %d and prev is %d\n\n", temp->type, temp->next, temp->prev);
	return 0;
}

/*
	lowest_inode_id = 25+1
	inode_ids p1 : 1  2  3  4  5  6  7  8  9  10 | free_ids : 0 | next_page = p2
	inode_ids p2 : 11 12 13 14 15 16 17 18 19 20 | free_ids : 0 | next_page = p3
	inode_ids p3 : 21 22 23 24 25    27 28 29 30 | free_ids : 1 | next_page = NULL
*/
struct volume_control *get_volume_control()
{
	struct volume_control *volume_table = malloc(VOLUME_CONTROL_SIZE);
	if (!volume_table)
	{
		return NULL;
	}
	if(readblock(volume_table, 0, VOLUME_CONTROL_SIZE) < 0)
	{
		free(volume_table);
		return NULL;
	}
	return volume_table;
}

ino_t get_inode_id()
{
	struct volume_control *table = get_volume_control();
	if (!table)
	{
		return -EFAULT;
	}

	ino_t lowest_inode_id = 1;
	struct inode_page *temp_inode_page = malloc(sizeof(struct inode_page));
	if (!temp_inode_page)
	{
		return -ENOMEM;
	}

  readblock(temp_inode_page, table->inode_block, INODE_PAGE_SIZE);

	while(temp_inode_page->free_ids == 0){ // if empty, look for next table
		if (temp_inode_page->next_page == 0){
			free(temp_inode_page);
			return -ENOMEM;
		}
		else
		{
			lowest_inode_id += 5;
			readblock(temp_inode_page, temp_inode_page->next_page, INODE_PAGE_SIZE);
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

	struct inode_page *temp_inode_page = malloc(INODE_PAGE_SIZE);
	if (!temp_inode_page) {
		free(table);
		return -ENOMEM;
	}
	if(readblock(temp_inode_page, table->inode_block, INODE_PAGE_SIZE) < 0)
	{
		free(table);
		free(temp_inode_page);
		return -EFAULT;
	}

	ino_t max_page_inode = 5;
	while (id > max_page_inode)
	{
		if (temp_inode_page->next_page == 0) // no next page
		{
			free(table);
			free(temp_inode_page);
			return -EFAULT;
		}
		if (readblock(temp_inode_page, temp_inode_page->next_page, INODE_PAGE_SIZE) < INODE_PAGE_SIZE)
		{
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
	ino_t inode_page_id = id % 5;

	// clear inode array index
	memset(&temp_inode_page->inodes[inode_page_id], 0, INODE_PAGE_SIZE);

	free(temp_inode_page);
	return 0;
}

ino_t update_inode(int size){
	ino_t id = get_inode_id();

	struct inode_page *temp_inode_page = malloc(INODE_PAGE_SIZE);
	if (!temp_inode_page) {
		return -ENOMEM;
	}

	ino_t index = get_inode_page(temp_inode_page, id);
	if (index < 0)
	{
		return -EAGAIN;
	}
	struct inode *temp_inode = &temp_inode_page->inodes[index];
	temp_inode->size = 0;


	return (ino_t) 0;
}

int fs_getattr( const char *path, struct stat *stbuf ) {
	// printf("getattr : start\n");
	// printf("getattr: (path=%s)\n", path);

	//memset(stbuf, 0, sizeof(struct stat)); // empty the stat struct

	// printf("getattr : about to allocate root.\n");
  struct linkedlist_dir *root = malloc(LINKEDLIST_SIZE);
	if (!root)
	{
		// printf("getattr : failed to allocate root\n");
		return -ENOMEM;
	}
	if (readblock(root, 2, LINKEDLIST_SIZE) < 0)
	{
		// printf("getattr : failed to read root\n");
		free(root);
		return -EFAULT;
	}

	// printf("getattr : done allocating.\n");
	// printf("getattr : About to get path.\n");

	struct linkedlist_dir *node = get_link(path);
  if(!node)
  {
		// printf("getattr : failed getting path (%s)\n", path);
    return -ENOENT;
  }
	// printf("getattr : Done getting path.\n");

  if(node->type == 0)
  {
    stbuf->st_mode = S_IFMT; //is file
  }
  else
  {
    stbuf->st_mode = S_IFDIR; //is directory
  }

	// printf("getattr : about to allocare for inode page.\n");
  struct inode_page *page = malloc(INODE_PAGE_SIZE);
  get_inode_page(page, node->file_inode);
  int page_num = node->file_inode % 5; //get the correct index  of the inode on the current page
  free(node);

	// printf("getattr : setting inode up.\n");
  stbuf->st_size = page->inodes[page_num].size;
  stbuf->st_atim = page->inodes[page_num].atime;
  stbuf->st_mtim = page->inodes[page_num].mtime;

	return 0;
}

int fs_readdir( const char *path,
                void *buf,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi )
{
	(void) offset;
	(void) fi;
 	printf("readdir : (path=%s)\n", path);


  struct linkedlist_dir *root = malloc(LINKEDLIST_SIZE);
	if (!root) {
		printf("root allocation failed\n");
		return -ENOMEM;
	}

  readblock(root, 2, LINKEDLIST_SIZE);

	printf("ino_t of root is: %ld \n", root->file_inode);
	struct linkedlist_dir *listlink = get_link(path);

	filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

	if(strcmp(basename(path), "/") == 0){

		filler(buf, "test", NULL , 0);
		return 0;
	}

	// while(listlink)
  // {
  //   char name_buf[220];
  //   strcpy(name_buf, listlink->name);
	//
  //   if(strcmp(strcat(dirname(name_buf),"/"), path) == 0)
  //   {
  //     filler(buf, basename(listlink->name), NULL, 0);
  //   }
  //   readblock(listlink, listlink->next, LINKEDLIST_SIZE);
  // }

	return 0;
}

//Permission
int fs_open( const char *path,
						 struct fuse_file_info *fi )
{

  // printf("open: (path=%s)\n", path);
	return 0;
}

int fs_read(const char *path,
            char *buf,
            size_t size,
            off_t offset,
            struct fuse_file_info *fi )
{
  //should return num of bytes read (the number requsted on success)
	return 0;
}


int fs_write( const char *path,
							const char *buf,
							size_t size,
							off_t offset,
							struct fuse_file_info *fi)
{
	if (size % 512)
	{

	}
  return 0;
}

int fs_release(const char *path, struct fuse_file_info *fi) {

	// printf("release: (path=%s)\n", path);
	return 0;
}

int fs_mkdir(const char *path,
	 					 mode_t mode)
{
  return 0;
}
int fs_rmdir(const char *path, mode_t mode){
  return 0;
}

int fs_utime(const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
  return 0;
}

int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi){
  return 0;
}


int main( int argc, char *argv[] ) {
	// printf("Running lfs.\n");
	init_volume(20480, 512, 30);
	fuse_main( argc, argv, &lfs_oper ); // Mounts the file system, at the mountpoint given

	return 0;
}
