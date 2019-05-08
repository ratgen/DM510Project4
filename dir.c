#include "dir.h"

static int init_tree(ino_t finode, FILE* fp)
{
  dirnode* root = malloc(sizeof(dirnode));

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

  fwrite(root)
  free(root);
}


static int add_dirnode(dirnode *root, inot_t finode, int ftype, char *fname, FILE* fp)
{
  dirnode* temp_node = malloc(sizeof(dirnode));

  if (!tempnode)
  {
    return -1;
  }

  tempnode->name = fname;
  tempnode->type = ftype;

}
