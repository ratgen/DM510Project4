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

 int add_entry(int prev_linkedlist_id, char* fname, ino_t finode, int ftype, int block_id)
{
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
  entry->prev = prev_linkedlist_id;

  if (writeblock(entry, block_id, LINKEDLIST_SIZE) < 0)
  {
    free(entry);
    return -EFAULT;
  }

  // update prev linkedlist link
  struct linkedlist_dir *temp_prev = malloc(LINKEDLIST_SIZE);
  if(!temp_prev)
  {
    return -ENOMEM;
  }
  if (readblock(temp_prev, prev_linkedlist_id, LINKEDLIST_SIZE) < 0)
  {
    free(temp_prev);
    return -EFAULT;
  }

  temp_prev->next = block_id; // update linked

  // write it back to disk
  if (writeblock(temp_prev, prev_linkedlist_id, LINKEDLIST_SIZE) < 0)
  {
    free(temp_prev);
    return -EFAULT;
  }

  free(temp_prev);
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
  // read out struct in
  struct linkedlist_dir *entry = malloc(LINKEDLIST_SIZE);
  if (!entry)
  {
    return -ENOMEM;
  }
  res = readblock(entry, block_id, LINKEDLIST_SIZE);
  if (res < 0)
  {
    free(entry);
    return -EFAULT;
  }

  struct linkedlist_dir *prev_entry = malloc(LINKEDLIST_SIZE);
  if (!prev_entry)
  {
    return -ENOMEM;
  }
  res = readblock(prev_entry, block_id, LINKEDLIST_SIZE);
  if (res < 0)
  {
    free(prev_entry);
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
  // printf("get_link : start | path %s\n", path);
  struct linkedlist_dir *node = malloc(LINKEDLIST_SIZE);
  if(!node)
  {
    return NULL;
  }

  if (readblock(node, 2, LINKEDLIST_SIZE) < 0)
  {
    // printf("get_link : failed to read node\n");
    free(node);
    return NULL;
  }

  // printf("get_link : while loop about to begin\n");
  // printf("get_link : path is %s, and node->name is %s\n", path, node->name);
  // printf("get_link : node->name size is %zd and node->name_length is %d\n", sizeof(node->name), node->name_length);

  while(strcmp(node->name, path) != 0)
  {
    if(node->next == 0)
    {
      // printf("get_link : failed. node->next==0\n");
      return NULL;
    }
    // printf("get_link : about to read\n");
    if (readblock(node, node->next, LINKEDLIST_SIZE) < 0)
    {
      // printf("get_link : failed to read node->next\n");
      return NULL;
    }
    // printf("get_link : done with readblock\n");
  }
  // printf("get_link : while loop done and don with get_link\n");
  return node;
}
