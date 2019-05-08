#include "lfs.h"

int readblock(int block_id) // reads 512 bytes from disk
{
	char buffer[512];
	int offset = 512*block_id;
  if(fread(buffer, 512, 1, fseek(fp, offset, SEEK_SET)) != 512)//fseek positions the stream
  {
    -EAGAIN;
  }
	return buffer;
}
/*
	lowest_inode_id = 14+1
	inode_ids : 1 2 3 4 5 6 7 8 9 10 11 12 13 14  16 17 18 19 20 21 22 23 24 25 26 27 28 29 30
*/

ino_t get_inode_id(struct volume_control *table)
{
	ino_t lowest_inode_id = 1;
	struct inode_page *temp_inode_page = malloc(sizeof(struct inode_page));
	if (!temp_inode_page)
	{
		return -ENOMEM;
	}
	temp_inode_page = readblock(table->inode_block);
	struct inode inode_check;

	for (size_t i = 0; i < 10; i++) {
		inode_check = temp_inode_page->inodes[i];
		if (lowest_inode_id == inode_check.inode_no)
		{
			lowest_inode_id++;
		}
	}
  lowest_inode_id++;
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
  FILE *fp = fopen(DISKNAME, "rb");
  if(!fp)
  {
    //the opening has failed
  }

  int rp;
  disk_block *in = malloc(sizeof(disk_block)); //change name
  while(size > blocksize){
    if(fread(in, 512, 1, fseek(fp, offset, SEEK_SET)) != 512)//fseek positions the stream
    {
      -EAGAIN; //maybew should be something else
    }
		memcpy(buf + rp, in->data, 508);
    size = size - 508;
    offset = in->next;
  }
  //should return num of bytes read (the number requsted on success)
	return 0;
}

void update_free_blocks(free_block_count *first_free, FILE* disk)
{
	int nblock = 0;
	while (&disk+(nblock*512) != NULL)
	{
		nblock++;
	}
	first_free->first_free_block;
}

int *get_free_block(free_block_count *first_free, FILE* disk)
{
	if (first_free->first_free_block == NULL)
	{
		return -10;
	}
	int free_block = first_free->first_free_block;
	free_block->nfree--;
	if (first_free->nfree == 0) // there are no more blocks
	{
		update_free_blocks(first_free, disk);
	}
	else
	{
		first_free->first_free_block+1; // there are continues blocks left
	}
	return free_block;
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
	fuse_main( argc, argv, &lfs_oper ); // Mounts the file system, at the mountpoint given

	return 0;
}
