#include <sys/types.h>

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

ino_t get_inode_id();
int delete_inode(ino_t id);
