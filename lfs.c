#include "lfs.h"
#include "dir.h"

/*
*  /path/path1 path2/file
*/

int writeblock(void* buf, int block_id, size_t size)
// writes `size` bytes from `buf` to `block_id` file block
{
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

	FILE* fp = fopen(DISKNAME, "w+b");
	int offset = 512*block_id;

	if (fseek(fp, offset, SEEK_SET) < 0)
	{
		return -EAGAIN;
	}

	if(fwrite(buf, 1, size, fp) < size){
		return -EAGAIN;
	}
	return size;
}

int readblock(void* buf, int block_id, size_t size)
// reads `size` bytes from disk into `buf`
{
	if (size < 0 || size > 512)
	{
		return -EINVAL;
	}
  if (block_id < 0)
	{
		return -EINVAL;
	}
	FILE* fp = fopen(DISKNAME, "r+b");
	int offset = 512*block_id;
	if (fseek(fp, offset, SEEK_SET) < 0)
	{
		return -EAGAIN;
	}

  if(fread(buf, size, 1, fp) < size)//fseek positions the stream
  {
    return -EAGAIN;
  }
	return size;
}

int init_volume(int nblocks, int nblock_size, int max_entries)
{
	struct volume_control *disk = malloc(sizeof(struct volume_control));
	if (!disk)
	{
		return -ENOMEM;
	}
	disk->blocks =nblocks;
	disk->block_size = nblock_size;
  disk->max_file_entries = max_entries;
	// init_tree(2);
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
			lowest_inode_id += 10;
			readblock(temp_inode_page, temp_inode_page->next_page, INODE_PAGE_SIZE);
		}
	}

	struct inode inode_check;

	for (size_t i = 0; i < 10; i++) {
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

ino_t get_inode_page(struct inode_page *buffer, ino_t id)
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

	ino_t max_page_inode = 10;
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
		id = id-10; // go to next page, decrease id
	}
	memcpy(buffer, temp_inode_page, INODE_PAGE_SIZE);
	free(table);
	free(temp_inode_page);
	return id;
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

// inode get_dir_path(const char* path)
// {
//
// }

int fs_getattr( const char *path, struct stat *stbuf ) {
	// int res = 0;
	// printf("getattr: (path=%s)\n", path);
	//
	// memset(stbuf, 0, sizeof(struct stat));
	// if( strcmp( path, "/" ) == 0 ) {
	// 	stbuf->st_mode = S_IFDIR | 0755;
	// 	stbuf->st_nlink = 2;
	// } else if( strcmp( path, "/hello" ) == 0 ) {
	// 	stbuf->st_mode = S_IFREG | 0777;
	// 	stbuf->st_nlink = 1;
	// 	stbuf->st_size = 12;
	// } else
	// 	res = -ENOENT;

	int res = 0;
	// struct inode inode = get_path_dir(path);


	return res;
}

int fs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, "hello", NULL, 0);
	filler(buf, "test123", NULL, 0);

	if (strcmp(path, "/") == 0){
		filler(buf, "root_file", NULL, 0);
	}

	return 0;
}

//Permission
int fs_open( const char *path,
						 struct fuse_file_info *fi )
{

  printf("open: (path=%s)\n", path);
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
	// FILE* fp = fopen(DISKNAME, "w+");
	//
	// // disk_block *block_for_file = malloc(sizeof(disk_block));
	// // memcpy()

  return 0;
}

int fs_release(const char *path, struct fuse_file_info *fi) {

	printf("release: (path=%s)\n", path);
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
	init_volume(20480, 512, 30);
	fuse_main( argc, argv, &lfs_oper ); // Mounts the file system, at the mountpoint given

	return 0;
}
