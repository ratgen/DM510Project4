#include "dir.h"
#include "lfs.h"

static int init_tree(int block_id)
{
  struct dirnode* root = malloc(sizeof(dirnode));
  if(!root)
  {
    return -ENOMEM;
  }

  root->fname = "/";
  root->file_inode = update_inode(0);
  root->type = 1;
  root->next = NULL;
  root->subdir = NULL;
  root->parent = NULL;

  if (writeblock(root, block_id, DIRNODE_SIZE) < DIRNODE_SIZE)
  {
    free(root);
    return -EAGAIN;
  }
  free(root);
  return 0;
}


static int add_dirnode(struct dirnode *node, int ftype, char *fname, FILE* fp)
{
  if(strlen(fname) < 2 || strlen(fname) > 220)
  {
    -EINVAL;
  }
  if(ftype != 0 && ftype != 1)
  {
    -EINVAL;
  }
  struct dirnode* temp_node = malloc(sizeof(struct dirnode));
  if(!temp_node)
  {
    return -ENOMEM;
  }
  temp_node->fname = fname;
  temp_node->type = ftype;

  struct volume_control control_block = malloc(sizeof(volume_control));
  if (!controlblock)
  {
    free(temp_node);
    return -ENOMEM;
  }
  controlblock = readblock(0, sizeof(struct volume_control));
  temp_node->file_inode = update_inode(controlblock);

  temp_node->next = NULL;
  temp_node->subdir = NULL;
  temp_node->parent =

}

static int add_dirnode(struct dirnode *node, int ftype, char *fname, FILE* fp)
{
  return 0;
}

static int remove_dirnode(struct dirnode *node, char *name)
{
  return 0;
}

static int find_dirnode(struct dirnode* root, char *path)
{
  return 0;
}
