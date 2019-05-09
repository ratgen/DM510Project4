#include "dir.h"
#include "lfs.h"
#include <string.h>


 int init_head(int block_id, ino_t finode)
{
  // printf("init head : Entered init head\n");
  struct linkedlist_dir *head = malloc(LINKEDLIST_SIZE);
  if (!head)
  {
    return -ENOMEM;
  }
  // printf("init head : Done allocating init head\n");
  // printf("init head : About to copy\n");
  char* name_ptr = "/";
  strncpy(head->name, name_ptr, sizeof(name_ptr));
  // printf("init head : Done to copy. head->name is %s\n", head->name);
  head->name_length = 1;
  // printf("init_head : name_length is %d\n", head->name_length);
  head->type = 1;
  head->file_inode = finode;
  head->next = 0;
  head->block = block_id;

  // printf("init head : About to write block\n");
  if (writeblock(head, block_id, LINKEDLIST_SIZE) < 0 )
  {
    // printf("init head : Write block failed\n");
    free(head);
    return -EFAULT;
  }
  // printf("init head : done to write block\n");

  // printf("init_head : name is %s, and length is %d\n", head->name, head->name_length);

  // printf("init head : Head pointer is %p\n", &head);
  free(head);
  // printf("init head : done\n");
  return 0;
}

 int add_entry(int root_block_id, const char* fname, ino_t finode, int ftype, int block_id)
{
  struct linkedlist_dir *tail_node =  readblock(root_block_id, LINKEDLIST_SIZE);
  if(tail_node < 0)
  {
    return -EFAULT;
  }
  while(tail_node->next != 0)
  {
    tail_node = readblock(tail_node->next, LINKEDLIST_SIZE);
  }

  struct linkedlist_dir *entry = malloc(LINKEDLIST_SIZE);
  if (!entry)
  {
    return -ENOMEM;
  }
  char* name_ptr = fname;
  strcpy(entry->name, fname);
  entry->name_length = strlen(fname);
  entry->file_inode = finode;
  entry->type = ftype;
  entry->next = 0;
  entry->prev = tail_node->block;

  if (writeblock(entry, block_id, LINKEDLIST_SIZE) < 0)
  {
    free(entry);
    return -EFAULT;
  }
  free(entry);

  tail_node->next = block_id; // update linked

  // write it back to disk
  if (writeblock(tail_node, tail_node->block, LINKEDLIST_SIZE) < 0)
  {
    free(tail_node);
    return -EAGAIN; //indicates we need to delete entry already written
  }

  free(tail_node);
  return 0;
}

/*
* 2 cases:
* C1 -> FILE - CURRENT_LINK - EMPTY
* C2 -> FILE - CURRENT_LINK - FILE
*/
 int delete_entry(int block_id)
{
  int res = 0;
  struct linkedlist_dir *entry = readblock(block_id, LINKEDLIST_SIZE);
  if (entry < 0)
  {
    free(entry);
    return -EFAULT;
  }

  struct linkedlist_dir *prev_entry = readblock(block_id, LINKEDLIST_SIZE);
  if (prev_entry < 0)
  {
    return -EFAULT;
  }
  // C1
  if (entry->next == 0)
  {
    prev_entry->next = 0; // remove from prev_link to current_link
    res = writeblock(prev_entry, block_id, LINKEDLIST_SIZE);
    if (res < 0)
    {
      free(entry);
      free(prev_entry);
      return -EFAULT;
    }

    res = delete_inode(entry->file_inode);
    free(entry); // not longer needed
    free(prev_entry);

    if (res< 0)
    {
      return -EFAULT;
    }

    if (delete_block(block_id) < 0) // delete
    {
      return -EFAULT;
    }
  }
  return 0;
}

struct linkedlist_dir *get_link(const char* path)
{
  printf("get_link : start | path %s\n", path);
  struct linkedlist_dir *node = readblock(1, LINKEDLIST_SIZE);
  if (node < 0)
  {
    return -EFAULT;
  }
  while(strcmp(node->name, path) != 0)
  {
    if(node->next == 0)
    {
      return NULL;
    }
    node = readblock(node->next, LINKEDLIST_SIZE);
    if (node < 0)
    {
      return -EFAULT;
    }
  }
  return node;
}
