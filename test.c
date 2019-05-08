#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct disk_block{
	struct disk_block *next;
	char* data; // blocksize -4 for the next block pointer
};

struct inode{
  ino_t inode_no;
  size_t size;
  time_t atime;     //access time
  time_t mtime;      //modification time
	struct disk_block* start;
	struct disk_block* end;
};

struct dirnode{
  // char name[216]; // file path
  ino_t file_inode;
  int type; // file if 0, 1 if directory
  struct dirnode *next;
  struct dirnode *subdir;
  struct dirnode *parent;
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
};

int main(){
  int size_dir = (int) sizeof(struct dirnode);
  int size_inode = (int) sizeof(struct inode);
  int size_table_inode = (int) sizeof(struct inode_page);

  printf("The size of struct dirnode is: %d\n", size_dir);
  printf("The size of struct inode is: %d\n", size_inode);
  printf("The size of struct table inode is: %d\n", size_table_inode);

  return 0;
}
