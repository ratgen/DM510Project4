#include <string.h>

#define DIRNODE_SIZE sizeof(struct dirnode)
#define MAXDIRENTRY 30

/*
* structure representing chapter 13.3.3 directory structure
*/

#define FOLDER 0
#define FILE 1


struct dirnode{
  char name[220]; // absolute file path
  int name_length;
  ino_t file_inode;
  int type;       // file if 0, 1 if directory
  struct dirnode *next;
  struct dirnode *subdir;
  struct dirnode *parent;
};

static int init_tree(ino_t finode, FILE* fp);

static int add_dirnode(dirnode *node, inot_t finode, int type, char *fname, FILE* fp);

static int remove_dirnode(dirnode *node, char *name);

static int find_dirnode(dirnode* root, char *path);
