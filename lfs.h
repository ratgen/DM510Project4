#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libgen.h>

#define BLOCKSIZE 512
#define INODE_PAGE_SIZE sizeof(struct inode_page)
#define VOLUME_CONTROL_SIZE sizeof(struct volume_control)
#define DISK_BLOCK_SIZE sizeof(struct disk_block)

FILE * file_system;

int fs_getattr( const char *, struct stat * );
int fs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int fs_open( const char *, struct fuse_file_info * );
int fs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int fs_release(const char *path, struct fuse_file_info *);
int fs_write( const char *, const char *, size_t, off_t, struct fuse_file_info *);
int fs_mkdir(const char *, mode_t);
int fs_rmdir(const char *, mode_t);
int fs_utime(const char *, const struct timespec tv[2], struct fuse_file_info *);
int fs_truncate(const char *, off_t, struct fuse_file_info *);

static struct fuse_operations lfs_oper = {
	.getattr	= fs_getattr,
	.readdir	= fs_readdir,
	.mkdir = fs_mkdir,
	.rmdir = fs_rmdir,
	.truncate = fs_truncate,
	.open	= fs_open,
	.read	= fs_read,
	.release = fs_release, //closes a file
	.write = fs_write,
	.utime = fs_utime,     //maybe should use uitemns (used to update access and modification of file)
};
// test/
//		hej/
//			file1.c
//		file.c

// test/
//		file.c
//		hej/
//			file1.c

struct disk_block{
	int next_block;
	char data[508]; // blocksize -4 for the next block pointer
};

struct inode{
  ino_t inode_no;
  size_t size;
  struct timespec atime;     //access time
  struct timespec mtime;      //modification time
	struct disk_block* start;
	struct disk_block* end;
};

struct inode_page{ // fills out a block of memory (48*10+16)
  struct inode inodes[7];
  int size;
	int free_ids;
  int next_page; // use block number here
};

struct volume_control{
  int blocks;
  int free_block_count;
  int block_size;
	int inode_block;
	int max_file_entries;
};

ino_t get_inode_id();
int delete_inode(ino_t id);
int delete_block();
int writeblock(void* buf, int block_id, size_t size);
int readblock(void* buf, int block_id, size_t size);
