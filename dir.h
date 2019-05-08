#include <string.h>

#define DIRNODE_SIZE sizeof(struct dirnode)
#define MAXDIRENTRY 30

// #define FOLDER    0
// #define FILE      1

/*
* structure representing chapter 13.3.3 directory structure
*/

struct dirnode{
  char name[220]; // absolute file path
  int name_length;
  ino_t file_inode;
  int type;       // file if 0, 1 if directory
  struct dirnode *next;
  struct dirnode *subdir;
  struct dirnode *parent;
};

static int init_tree(int block_id);

static int add_dirnode(struct dirnode *node, int ftype, char *fname, FILE* fp);

static int remove_dirnode(struct dirnode *node, char *name);

static int find_dirnode(struct dirnode* root, char *path);
