#include "dir.h"

static int init_tree(ino_t finode, FILE* fp)
{
  struct dirnode* root = malloc(sizeof(dirnode));

  if (!root)
  {
    return -1;
  }

  dirnode->fname = "/";
  dirnode->file_inode = finode;
  dirnode->type = 1;
  dirnode->next = NULL;
  dirnode->subdir = NULL;
  dirnode->parent = NULL;

  fwrite(root, sizeof(dirnode), )
  free(root);
}


static int add_dirnode(dirnode *node, inot_t finode, int ftype, char *fname, FILE* fp)
{
  struct dirnode* temp_node = malloc(sizeof(struct dirnode));


}
