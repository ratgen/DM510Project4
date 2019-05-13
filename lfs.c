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
};

void print_unsigned_binary(unsigned int n)
{
  if (n > 1)
  print_unsigned_binary(n/2);
  printf("%d", n % 2);
}

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

int init_bitmap()
{
  union lfs_block* bitmap_block = malloc(sizeof(union lfs_block));
  //Write empty blocks to the 2nd through fifth blocks
  for (size_t i = 1; i < 5; i++) {
    writeblock(bitmap_block, i);  //The first 5 blocks are allocated to the bitmap
  }
  //Reserve the first to 6th block for, (bitmap * 5, root inode)
  unsigned char temp_byte = 0;
  for (size_t i = 0; i < 6; i++) {
     temp_byte |= 1 << i;
  }
  memcpy(bitmap_block->data[0], temp_byte, sizeof(char));
  writeblock(bitmap_block, 0);
}

unsigned int get_block()
{
  union lfs_block* bitmap_block;
  unsigned int free_block;
  int k = 0;
  while(free_block != 0){
    bitmap_block = readblock(k);
    for (size_t i = 0; i < 512; i++)
    {
      if(bitmap_block->data[i] != 255)
      {
        free_block = i + (k+1)*512;
        break;
      }
    }
    k += 1;
  }
  return free_block;
}

int free_block(unsigned int block)
{
  //Floor division to find the correct page
  int page = block/4096;

  //Find the correct indice in the array
  int bank = (block % 4096)/8;
  //Read the bank, to be manipulated
  union lfs_block* bitmap_block = readblock(page);
  unsigned char temp_byte;
  memcpy(temp_byte, bitmap_block->data[bank], sizeof(char));
  //Toggle the bit
  temp_byte ^= (1UL << (block % 8));
  //Copy the bank back, and write it to the disk
  memcpy(bitmap_block->data[bank], temp_byte, sizeof(char));
  writeblock(bitmap_block, page);
}

int lfs_getattr( const char *path, struct stat *stbuf )
{
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


int main( int argc, char *argv[] )
{
  file_system = fopen("file", "r+");

  init_bitmap();

  union lfs_block* k = readblock(0);

  //memcpy(b->data, &data, 8);


	//fuse_main( argc, argv, &lfs_oper );

	return 0;
}
