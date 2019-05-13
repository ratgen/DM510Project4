#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

FILE * file_system;

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, struct fuse_file_info *fi);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = NULL,
	.mkdir = NULL,
	.unlink = NULL,
	.rmdir = NULL,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = NULL,
	.rename = NULL,
	.utime = NULL
};

typedef struct lfs_inode //sizeof() = 512
{
  struct timespec a_time; //set with clock_gettime(CLOCK_REALTIME, &timespace a_time)
  struct timespec m_time; //16 bytes

  //Parents are referenced by their block num of their inode, as a 2 bytes integer (within the range of the num of blocks)
  unsigned short parent;  // id of the parent //2 bytes
  //chilren of folders are contained the in the data block
  unsigned char type;     //1 byte
  unsigned int size;  //4 bytes
  unsigned short data[236]; //array of block ids can hold 236*512 = 0.24 MB
} inode_t;

typedef union lfs_block  //with union block can either be data or inode_t
{
  inode_t inode;
  unsigned char data[512];
} block;

int writeblock(void* buf, int block)
{
	if (buf == NULL)
	{
		return -EINVAL;
	}
	if (block < 0)
	{
		return -EINVAL;
	}
	int offset = 512*block;
	fseek(file_system, offset, SEEK_SET);
	if(fwrite(buf, 512, 1, file_system) != 1)
  {
		return -EAGAIN;
	}
	return 1;
}

void* readblock(int block)
// reads `size` bytes from disk and returns a void* pointer to the data
{
  if (block < 0)
	{
		return -EINVAL;
	}
  void* buffer = malloc(sizeof(char) * 512);
  if(!buffer)
  {
    return -ENOMEM;
  }

	int offset = 512*block;
  if(fseek(file_system, offset, SEEK_SET) < 0){
    free(buffer);
    return -EAGAIN;
  }
  if(fread(buffer, 512, 1, file_system) != 1)
  {
    free(buffer);
    return -EAGAIN;
  }
	return buffer;
}

int lfs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	if( strcmp( path, "/" ) == 0 ) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if( strcmp( path, "/hello" ) == 0 ) {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = 12;
	} else
		res = -ENOENT;

	return res;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, "hello", NULL, 0);

	return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	memcpy( buf, "Hello\n", 6 );
	return 6;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int main( int argc, char *argv[] ) {
  file_system = fopen("file", "r+");
  printf("size: %d\n", sizeof(inode_t));

  block* b = malloc(sizeof(block));
  strcpy(b->data, "This is a thing");
  writeblock(b, 0);

  block* c = readblock(0);
  printf("%s\n", c->data);

	//fuse_main( argc, argv, &lfs_oper );

	return 0;
}
