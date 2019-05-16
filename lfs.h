#include <math.h>
#include <fuse.h>
 #include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#define INODE_BLOCK_IDS 231
#define LFS_BLOCK_SIZE 512

FILE * file_system;

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_write( const char *, const char *, size_t, off_t, struct fuse_file_info *);
int lfs_mkdir(const char* path, mode_t mode);
int lfs_rmdir(const char* path);
int lfs_create(const char* path, mode_t mode, struct fuse_file_info *fi);
int lfs_unlink(const char *path);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
 	.mkdir = lfs_mkdir,
	.unlink = lfs_unlink,
  .create = lfs_create,
	.rmdir = lfs_rmdir,
	.open	= lfs_open,
	.read	= lfs_read,
	.write = lfs_write
};

typedef struct lfs_inode //sizeof() = LFS_BLOCK_SIZE
{
  //test with ls -ul (lists access time) | test with ls -l (lists modification time)
  struct timespec a_time; //set with clock_gettime(CLOCK_REALTIME, &timespace a_time)
  struct timespec m_time; //16 bytes

  //Parents are referenced by their block num of their inode, as a 2 bytes integer (within the range of the num of blocks)
  unsigned short parent;  // id of the parent //2 bytes
  //chilren of folders are contained the in the data block
  unsigned char type;     //1 byte
  int size;  //4 bytes
  int blocks; //blocks allocated to this inode
  int name_length;
  unsigned short data[INODE_BLOCK_IDS]; //array of block ids can hold 232*LFS_BLOCK_SIZE = 0.24 MB
} inode_t;

typedef union lfs_block  //with union block can either be data or inode_t
{
  inode_t inode;
  unsigned char data[LFS_BLOCK_SIZE];

} lfs_block;
