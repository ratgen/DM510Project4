#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>

struct disk_block{
	int next_block;
	char* data; // blocksize -4 for the next block pointer
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
  struct inode inodes[5];
	int free_ids;
  int next_page; // use block number here
};

struct volume_control{
  int blocks;
  int free_blocks;
  int block_size;
  int free_block;
	int inode_block;
	int max_file_entries;
};

int main(){
  printf("INODE_PAGE_SIZE %zd\n", sizeof(struct inode_page));
  printf("INODE_SIZE %zd\n", sizeof(struct inode));
  printf("INODE_SIZE %zd\n", sizeof(struct disk_block));
  printf("TIMESPEC_SIZE %zd\n", sizeof(struct timespec));
  printf("SIZE_T_SIZE %zd\n", sizeof(size_t));
  printf("INO_T_SIZE %zd\n", sizeof(ino_t));

}
