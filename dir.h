#include <string.h>
#include <sys/types.h>

#define MAXDIRENTRY 30
#define LINKEDLIST_SIZE sizeof(struct linkedlist_dir)

struct linkedlist_dir{
  char name[220]; // absolute file path
  int name_length;
  ino_t file_inode;
  int type;       // file if 0, 1 if directory
  int next;
  int prev;
};

int init_head(int block_id, ino_t finode);

int add_entry(int prev_linkedlist_id, char* fname, ino_t finode, int ftype, int block_id);

int delete_entry(int block_id);

struct linkedlist_dir *get_link(const char* path);
