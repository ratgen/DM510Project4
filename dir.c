#include "dir.h"

static int init_tree(ino_t finode, FILE* fp)
{
  struct dirnode* root = malloc(sizeof(dirnode));

  if (!root)
  {
    return -1;
  }

  dirnode->fname = "/";
  dirnode->inode = finode;
  dirnode->type = 1;
  dirnode->next = NULL;
  dirnode->subdir = NULL;
  dirnode->parent = NULL;

  fwrite(root, sizeof(dirnode), )
  free(root);
}


static int add_dirnode(dirnode *node, inot_t finode, int ftype, char *fname, FILE* fp)
{ //lav en ny inode?
  if(strlen(fname) < 2)
  {
    -EINVAL;
  }
  if(ftype != 0 && ftype != 1)
  {
    -EINVAL;
  }
  struct dirnode* temp_node = malloc(sizeof(struct dirnode));
  temp_node->fname = fname;
  temp_node->type = ftype;
  temp_node->file_inode = finode;


}
