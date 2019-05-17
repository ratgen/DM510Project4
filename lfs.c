#include "lfs.h"

int writeblock(void* buf, int block)
{
	if (buf == NULL)
	{
		return -EINVAL;
	}
  if (block < 0)
  {
    return -EINVAL;
  }
  int offset = LFS_BLOCK_SIZE*block;
  fseek(file_system, offset, SEEK_SET);
  if(fwrite(buf, LFS_BLOCK_SIZE, 1, file_system) != 1)
  {
    return -EAGAIN;
  }
	return 1;
}

// reads `size` bytes from disk and returns a void* pointer to the data
void* readblock(int block)
{
  if (block < 0)
	{
		return (void*) -EINVAL;
	}
  void* buffer = calloc(1, sizeof(char) * LFS_BLOCK_SIZE);
  if(!buffer)
  {
    return (void*) -ENOMEM;
  }

	int offset = LFS_BLOCK_SIZE*block;
  if(fseek(file_system, offset, SEEK_SET) < 0){
    free(buffer);
    return (void*) -EAGAIN;
  }
  if(fread(buffer, LFS_BLOCK_SIZE, 1, file_system) != 1)
  {
    free(buffer);
    return (void*) -EAGAIN;
  }
	return buffer;
}

int init_bitmap()
{
  union lfs_block* bitmap_block = calloc(1, sizeof(union lfs_block));
  if(!bitmap_block)
  {
    return -ENOMEM;
  }
  //Write empty blocks to the 2nd through fifth blocks
  for (size_t i = 1; i < 5; i++) {
    writeblock(bitmap_block, i);  //The first 5 blocks are allocated to the bitmap
  }
  //Reserve the first to 6th block for, (bitmap * 5, root inode)
  unsigned char temp_byte = 0;
  for (size_t i = 0; i < 6; i++) {
     temp_byte |= 1 << i;
  }
  memcpy(&bitmap_block->data[0], &temp_byte, sizeof(char));
  writeblock(bitmap_block, 0);
  free(bitmap_block);
  return 0;
}

int get_block()
{
  union lfs_block* bitmap_block;
  unsigned int free_bank = -1;
  int k;
  for (k = 0; k < 5; k++) {   					//THIS CAN BE DONE WITH WHILE?
    bitmap_block = readblock(k);
    if(bitmap_block < 0)
    {
      return bitmap_block;
    }
    for (size_t i = 0; i < LFS_BLOCK_SIZE; i++)			//THIS CAN BE DONE WITH WHILE?
    {
      if(bitmap_block->data[i] < 255)  //note that 255 is the value of an unsigned byte with all bits set
      {
        //found a byte, where there is some free bits
        free_bank = i;
        break;
      }
     }
    if(free_bank != -1)
    {
      if(k == 4)
      {
        return -ENOSPC;
      }
      break;
    }
    free(bitmap_block);
    printf("GET BLOCK k: %d\n", k);
  }
  //the right bank has been obtained, now copy the bank in, such that is can be maipulated
  unsigned char temp_byte = 0;
  memcpy(&temp_byte, &bitmap_block->data[free_bank], sizeof(char));
  int bit = 0;
  for (size_t i = 0; i < 8; i++) {
     //shifts temp_byte[i] into bit
     bit = (temp_byte >> i) & 1U;
     if(bit == 0)
     {
      bit = i;
      temp_byte |= 1 << bit;
      break;
    }
  }
  //write the new bitmap to disk, and return the number
  memcpy(&bitmap_block->data[free_bank], &temp_byte, sizeof(char));
  writeblock(bitmap_block, k);
  free(bitmap_block);
  return k*4096 + free_bank*8 + bit;
}

int free_block(unsigned int block)
{
  //Floor division to find the correct block
  int data_block = block/4096;
  //Find the correct indice in the array
  int bank = (block % 4096)/8;
  //Read the bank, to be manipulated
  union lfs_block* bitmap_block = readblock(data_block);
  if(bitmap_block < 0)
  {
    return bitmap_block;
  }
  unsigned char temp_byte;
  memcpy(&temp_byte, &bitmap_block->data[bank], sizeof(char));
  //Toggle the bit
  temp_byte ^= (1UL << (block % 8));
  //Copy the bank back, and write it to the disk
  memcpy(&bitmap_block->data[bank], &temp_byte, sizeof(char));
  writeblock(bitmap_block, data_block);
  free(bitmap_block);
  return 0;
}

//setup the root node
int setup()
{

    init_bitmap();
    union lfs_block* disk_block = calloc(1, sizeof(union lfs_block));
    if(!disk_block)
    {
      return -ENOMEM;
    }
  	disk_block->inode.parent = 0;
  	disk_block->inode.type = 1;
    disk_block->inode.blocks = 2;

    //Get block for the name of root node
  	int block_id = get_block(); //get first block
    if(block_id < 0)
    {
      free(disk_block);
      return block_id;
    }
    disk_block->inode.data[0] = block_id;
  	union lfs_block *name = calloc(1, LFS_BLOCK_SIZE);
    if(!name)
    {
      return -ENOMEM;
    }
    //Insert name length into inode
    disk_block->inode.name_length = 2;
    //Copy name of root to data block
    memcpy(name,"/", sizeof(char)*disk_block->inode.name_length);

  	writeblock(name, disk_block->inode.data[0]);
    free(name);

    //Set times for access and modification
  	clock_gettime(CLOCK_REALTIME, &disk_block->inode.a_time);
  	clock_gettime(CLOCK_REALTIME, &disk_block->inode.m_time);

    //Write the disk block
  	writeblock(disk_block, 5);
    free(disk_block);


	return 0;
}

//search for directory or file with "name" in directory at "search_block_id"
int get_name(unsigned int search_block_id, char* name)
{
   //read the block to search for names
  union lfs_block* search_block = readblock(search_block_id);
  if(search_block < 0)
  {
    return search_block;
  }

  int i;
  int cand_block;
  for (i = 1; i < INODE_BLOCK_IDS; i++)
  {
    //read the address of the i'th data block
    cand_block = search_block->inode.data[i];
    if(cand_block > 0 && cand_block < 20480) //sanity check the candidate
    {
      //inode of the candidate is read
      union lfs_block* cand_inode = readblock(cand_block);
      if(cand_inode < 0)
      {
        return cand_inode;
      }
      //read the candidate's name block
      union lfs_block* name_block = readblock(cand_inode->inode.data[0]);
      if(name_block < 0)
      {
        return name_block;
      }
      //copy the name into array to compare
      char data[cand_inode->inode.name_length];
      memcpy(&data, &name_block->data, cand_inode->inode.name_length);
      //if the names and data match, then the candidate block is correct
      if(strcmp(data, name) == 0)
      {
        free(cand_inode);
        free(name_block);
        free(search_block);
        return cand_block;
      }
      free(cand_inode);
      free(name_block);

      printf("GET_NAME: names did not match\n");
    }
  }
  //did not find the name in the dir
  free(search_block);
  return -ENOENT;
}

int get_block_from_path(const char* path)
{
  char path_save[LFS_BLOCK_SIZE];
  strcpy(path_save, path); //save the path, as strtok() modifies path

  char *path_element;
  int block_id;
  //If the path is root folder
  if(strcmp(path, "/") == 0)
  {
    printf("%s\n", "GET_BLOCK_PATH: Returning root node");
    return 5;
  }
   //get the path of the first folder or file
  path_element = strtok((char *) path, "/");
  //gets the block of the path_element in the root block
  block_id = get_name(5, path_element);
  if(block_id < 6)
  {
    //path does not exist
    return -ENOENT;
  }
  while( path_element != NULL )
  {
    //split next part of path
    path_element = strtok(NULL, "/");
    if(path_element == NULL)
    {//there is no next element, block contains the sought after block
      return block_id;
    }
    block_id = get_name(block_id, path_element);
    if(block_id < 6)
    {
      //a block for name could not be found
      return block_id;
    }
  }
  //copy the path back, such that it is not modified in the function calling
  strcpy((char*) path, path_save);
  return -ENOENT;
}

//get a free slot for a block pointer in an inode
int get_free_slot_dir(union lfs_block* block)
{
  int free_slot = 0;
  //block pointers must pass sanity check before being comfirmed as blocks
  while(block->inode.data[free_slot] != 0 && block->inode.data[free_slot] < 20480)
  {
    printf("FREE SLOT: content of freeslot %d: %d\n",free_slot, block->inode.data[free_slot]);
    free_slot += 1;
  }
  if(free_slot > INODE_BLOCK_IDS)
  {
    printf("%s\n", "no more free slots");
    return -EFBIG;
  }
  printf("FREE SLOT: returning %d\n", free_slot);
  return free_slot;
}

//updates of the number of blocks and bytes used by children
int set_num_blocks(int block, int dif_blocks, int dif_size)
{
  printf("SET NUM BLOCKS: %d, difference_blocks %d, difference_size %d\n", block, dif_blocks, dif_size);
  union lfs_block* child_inode = readblock(block);
  if(child_inode < 0)
  {
    return child_inode;
  }
  union lfs_block* child_name = readblock(child_inode->inode.data[0]);
  if(child_name < 0)
  {
    return child_name;
  }
  //base case
  char name[512];
  memcpy(name, child_name->data, child_inode->inode.name_length);
  if(strcmp(name, "/") == 0)
  {
    union lfs_block* root_inode = readblock(5);
    if(root_inode < 0)
    {
      return root_inode;
    }
    printf("SET NUM BLOCKS: old: root_inode->inode.blocks %d\n", root_inode->inode.blocks);
    printf("SET NUM BLOCKS: old: root_inode->inode.size   %d\n",   root_inode->inode.size);

    root_inode->inode.blocks += dif_blocks;
    root_inode->inode.size += dif_size;

    printf("SET NUM BLOCKS: new: root_inode->inode.blocks %d\n", root_inode->inode.blocks);
    printf("SET NUM BLOCKS: new: root_inode->inode.size   %d\n",   root_inode->inode.size);

   writeblock(root_inode, 5);
   free(root_inode);
  }
  else
  {
    union lfs_block* parent_inode = readblock(child_inode->inode.parent);
    if(parent_inode < 0)
    {
      return parent_inode;
    }

    printf("SET NUM BLOCKS: old: parent_inode->inode.blocks %d\n", parent_inode->inode.blocks);
    printf("SET NUM BLOCKS: old: parent_inode->inode.size %d\n",   parent_inode->inode.size);


    parent_inode->inode.blocks += dif_blocks;
    parent_inode->inode.size += dif_size;

    printf("SET NUM BLOCKS: new: parent_inode->inode.blocks %d\n", parent_inode->inode.blocks);
    printf("SET NUM BLOCKS: new: parent_inode->inode.size %d\n",   parent_inode->inode.size);

    writeblock(parent_inode, child_inode->inode.parent);
    short parent = child_inode->inode.parent;

    free(parent_inode);
    free(child_inode);
    //recursivly call on parent
    set_num_blocks(parent, dif_blocks, dif_size);
  }
  printf("%s\n", "returning from SET NUM BLOCKS");
  return 0;
}

//gets attribute of file or directory
int lfs_getattr( const char *path, struct stat *stbuf )
{
	printf("getattr: (path=%s)\n", path);
	memset(stbuf, 0, sizeof(struct stat));
  //get the block id of the requested path
  int inode_id = get_block_from_path(path);
  if(inode_id < 0)
  {
    return inode_id;
  }
  union lfs_block* block = readblock(inode_id);
  if(block < 0)
  {
    return block;
  }
  //determine if folder or not, at set appropriate mode
  if(block->inode.type == 1)
  {
    stbuf->st_mode = S_IFDIR | 0777;
  }
  else if(block->inode.type == 0)
  {
    stbuf->st_mode = S_IFREG | 0777;
  }
  else
  {
    return -EINVAL;
  }
  stbuf->st_blksize = (long) 512;
  stbuf->st_size = block->inode.size;
  stbuf->st_blocks = block->inode.blocks;
  stbuf->st_atim = block->inode.a_time;
  stbuf->st_mtim = block->inode.m_time;

  free(block);

	return 0;
}

int lfs_mkdir(const char* path, mode_t mode)
{
  //get a new block for the inode
  int block = get_block();
  if(block < 0)
  {
    return block;
  }
  //save path, as it will be modified by path
  char save_path[LFS_BLOCK_SIZE];
  strcpy(save_path, path);
  //get block of parent dir and read it in
  unsigned short parent_block_id = get_block_from_path(dirname(save_path));
  union lfs_block* parent_block = readblock(parent_block_id);
  if(parent_block < 0)
  {
    return parent_block;
  }

  int free_slot = get_free_slot_dir(parent_block);
  if(free_slot < 0)
  {
    return free_slot;
  }
  //insert reference to the new dir into parent dir
  parent_block->inode.data[free_slot] = block;
  //Write the new parent to disk
  writeblock(parent_block, parent_block_id);
  free(parent_block);

  //setup the new inode for the dir
  union lfs_block* new_dir = calloc(1, LFS_BLOCK_SIZE);
  if(!new_dir)
  {
    return -ENOMEM;
  }

  new_dir->inode.parent = parent_block_id;
  new_dir->inode.type = 1;
  new_dir->inode.blocks = 2;
  new_dir->inode.size = 0;
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.m_time);

  //create block for the name, and save it in the new inode
  int block_id = get_block();
  if(block_id < 0)
  {
    free(new_dir);
    return block_id;
  }
  new_dir->inode.data[0] = block_id;
  union lfs_block* name_block = calloc(1,LFS_BLOCK_SIZE);
  if(!name_block)
  {
    return -ENOMEM;
  }

  //set the length of the name, and copy the name into its block
  new_dir->inode.name_length = strlen(basename((char *) path)) + 1;
  memcpy(name_block->data, basename((char *) path), new_dir->inode.name_length);


  //write the name, new dir and free the calloc'ed memory
  writeblock(name_block, new_dir->inode.data[0]);
  free(name_block);
  writeblock(new_dir, block);
  free(new_dir);

  set_num_blocks(block, 2, 0); //

  return 0;
}

int lfs_rmdir(const char* path)
{
  int rm_block_id = get_block_from_path(path);
  if(rm_block_id < 0)
  {
    return rm_block_id;
  }
  union lfs_block* rm_block = readblock(rm_block_id);
  if(rm_block < 0)
  {
    return rm_block;
  }
  if(rm_block->inode.type != 1)
  {
    return -ENOTDIR;
  }
  if(rm_block->inode.blocks > 2) //the two blocks are inode and name data
  {
    printf("number of blocks in folder %d\n", rm_block->inode.blocks);
    return -ENOTEMPTY;
  }
  set_num_blocks(rm_block_id, -2, 0);

  union lfs_block* rm_block_parent = readblock(rm_block->inode.parent);
  if(rm_block_parent < 0)
  {
    return rm_block_parent;
  }
  for(int i = 0; i < INODE_BLOCK_IDS; i++)
  {
    //search the parents block pointer, until the rm_block_id is found
    if(rm_block_parent->inode.data[i] == rm_block_id)
    {
      rm_block_parent->inode.data[i] = 0;
      break;
    }
  }
  //write parent to disk, and free the memory
  writeblock(rm_block_parent,rm_block->inode.parent);
  free(rm_block_parent);

  //free the name block, and the inode of the directory, and free the memory allocated to

  free_block(rm_block->inode.data[0]);
  free_block(rm_block_id);
  free(rm_block);

  return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi )
{
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  printf("readdir: (path=%s)\n", path);
  int dir_block_id = get_block_from_path(path);
  if(dir_block_id < 0)
  {
    return dir_block_id; //return error given by get_block_from_path
  }
  //read in inode of directory
  union lfs_block* dir_block = readblock(dir_block_id);
  if(dir_block < 0)
  {
    return dir_block;
  }
  if(!(dir_block->inode.type == 1))
  {
    return -ENOTDIR;
  }

  for (int i = 1; i < INODE_BLOCK_IDS; i++) {
    //Check if numbers are valid
    if(dir_block->inode.data[i] > 0 && dir_block->inode.data[i] < 20480)
    {
      printf("READDIR: found file or folder at %d, block %d\n", i, dir_block->inode.data[i]);
      //read in the inode of the dir or file
       union lfs_block* temp_block = readblock(dir_block->inode.data[i]);
       if(temp_block < 0)
       {
         return temp_block;
       }
      //read this inode's name block
      union lfs_block* name_block = readblock(temp_block->inode.data[0]);
      if(name_block < 0)
      {
        return name_block;
      }

      printf("READDIR: name is in block: %d\n", temp_block->inode.data[0]);
      //copy the name into array to pass to filler
      char data[temp_block->inode.name_length];
      memcpy(&data, &name_block->data, temp_block->inode.name_length);
      printf("%s\n", data);
      filler(buf, data, NULL, 0);

      free(temp_block);
      free(name_block);
    }
  }


	return 0;
}

int lfs_create(const char* path, mode_t mode, struct fuse_file_info *fi)
{
  char temp[LFS_BLOCK_SIZE];
  strcpy(temp, path);
  unsigned short parent_dir_id = get_block_from_path(dirname(temp));

  int new_file_id = get_block();
  if(new_file_id < 0)
  {
    return new_file_id;
  }
  //read the parent dir block
  union lfs_block* parent_dir = readblock(parent_dir_id);
  if(parent_dir < 0)
  {
    return parent_dir;
  }
  int free_slot = get_free_slot_dir(parent_dir);
  if(free_slot < 0)
  {
    return free_slot;
  }
  //insert new file id, and write to disk
  parent_dir->inode.data[free_slot] = new_file_id;
  writeblock(parent_dir, parent_dir_id);
  free(parent_dir);

  //allocate inode, for new file
  union lfs_block* new_file = calloc(1, LFS_BLOCK_SIZE);
  if(!new_file)
  {
    return -ENOMEM;
  }
  //allocate data block, for name
  union lfs_block* new_name = calloc(1, LFS_BLOCK_SIZE);
  if(!new_name)
  {
    free(new_file);
    return -ENOMEM;
  }
  //insert length of name into inode
  new_file->inode.name_length = strlen(basename((char *) path)) + 1;

  unsigned short new_name_id = get_block();
  if(new_name_id < 0)
  {
    free(new_file);
    free(new_name);
    return new_name_id;
  }
  //copy the name into the data block and write
  memcpy(new_name->data, basename((char *) path), new_file->inode.name_length);
  writeblock(new_name, new_name_id);
  free(new_name);

  //set name, type, parent, blocks used and size and clock
  new_file->inode.data[0] = new_name_id;
  new_file->inode.type = 0;
  new_file->inode.blocks = 2;
  new_file->inode.size = 0;
  new_file->inode.parent = parent_dir_id;
  clock_gettime(CLOCK_REALTIME, &new_file->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_file->inode.m_time);

  //write the file to disk, and update parents;
  writeblock(new_file, new_file_id);
  set_num_blocks(new_file_id, new_file->inode.blocks, new_file->inode.size);
  free(new_file);

  return 0;
}

int lfs_unlink(const char *path)
{
  //Get the block of path and read this block in
  int rm_block_id = get_block_from_path(path);
  union lfs_block* rm_block = readblock(rm_block_id);
  if(rm_block < 0)
  {
    return rm_block;
  }

  //update parents with the change in size
  set_num_blocks(rm_block_id, -rm_block->inode.blocks, -rm_block->inode.size);

  //read in parent, and remove its link to rm_block
  union lfs_block* rm_block_parent = readblock(rm_block->inode.parent);
  if(rm_block_parent < 0)
  {
    return rm_block_parent;
  }
  for(int i = 0; i < INODE_BLOCK_IDS; i++)
  {
    if(rm_block->inode.data[i] > 0 && rm_block->inode.data[i] < 20480)
    {
      printf("UNLINK: freeing block: \n", rm_block->inode.data[i]);
      free_block(rm_block->inode.data[i]);
    }
    if(rm_block_parent->inode.data[i] == rm_block_id)
    {
      rm_block_parent->inode.data[i] = 0;
    }
  }
  //write the updated parent, and free data blocks of name, and the inode
  writeblock(rm_block_parent, rm_block->inode.parent);
  free_block(rm_block_id);
  free(rm_block_parent);
  free(rm_block);
  return 0;
}


int lfs_open( const char *path, struct fuse_file_info *fi )
{
   //check if file exists
  int res = get_block_from_path(path);
  if(res > 0)
  {
   	return 0;
  }

  return -ENOENT;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi )
{
  //save the given offset
  int org_offset = offset;
  printf("READ: offset is: %d\n", org_offset);
  printf("READ: size  is: %ld\n", size);
  int retsize;

  //read in block of inode to read from
  unsigned int read_inode_id = get_block_from_path(path);
  union lfs_block* read_inode = readblock(read_inode_id);
  if(read_inode < 0)
  {
    return read_inode;
  }
  printf("READ: size of file %d\n", read_inode->inode.size);

  if(size > read_inode->inode.size)
  {
    //if the size to read is larger than the inode size, then read all data
    retsize = size;
    size = read_inode->inode.size;
  }

  //find the block, to start reading from
  int block_offset = offset/LFS_BLOCK_SIZE;
  printf("READ: start reading from block: %d\n", block_offset);
  //total number of blocks to read
  int num_blocks = (int) ceil((double) size/ (double) LFS_BLOCK_SIZE);
  printf("READ: total number of blocks to read: %d\n", num_blocks);

  //read in one block at a time from the block_offset, offset by 1 extra to adjust for the name block in inode data
  for(int i = block_offset + 1; i < num_blocks + block_offset + 1; i++)
  {
    union lfs_block* data_block = readblock(read_inode->inode.data[i]);
    if(data_block < 0)
    {
      return data_block;
    }
    //copy the whole data block into the buffer
    memcpy(&buf[offset - org_offset], &data_block->data[offset % LFS_BLOCK_SIZE], LFS_BLOCK_SIZE);
    if(size - offset > LFS_BLOCK_SIZE){
      offset += (long) LFS_BLOCK_SIZE - (offset % (long) LFS_BLOCK_SIZE);
    }
    else
    {
      offset += (long) size;
    }
    free(data_block);
  }
  //set the access timestamp, and write to the read_inode
  clock_gettime(CLOCK_REALTIME, &read_inode->inode.a_time);
  writeblock(read_inode, read_inode_id);
  free(read_inode);
  printf("READ: offset %ld bytes\n",  offset);
  return retsize;
}

int lfs_write( const char *path, const char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
  long org_offset = offset;
  int write_inode_id = get_block_from_path(path);
  if(write_inode_id < 0)
  {
    return -ENOENT;
  }
  union lfs_block* write_inode = readblock(write_inode_id);
  if(write_inode < 0)
  {
    return write_inode;
  }

  //save the sizes, such that the change in size can be calculated later
  int old_size = write_inode->inode.size;
  int old_blocks = write_inode->inode.blocks;

  //if the size of the of what to write is larger than total data blocks (excluding inode and name) already allocated, then allocate new blocks
  if((write_inode->inode.blocks - 2)*512 < offset + size )
  {
    //number of new data blocks to be allocated
     int new_blocks = (int)ceil((double)(offset + size - (write_inode->inode.blocks - 2)*512)/(double)LFS_BLOCK_SIZE);

    for(int i = 0; i < new_blocks; i++)
    {
      //read in the inode to allocate block to, and get a free block
      write_inode = readblock(write_inode_id);
      if(write_inode < 0)
      {
        return write_inode;
      }
      int block_id = get_block();
      if(block_id < 0)
      {
        free(write_inode);
        return block_id;
      }
      //we get a free slot in the inode, and write the new block_id to this
      int free_slot = get_free_slot_dir(write_inode);
      if(free_slot < 0)
      {
        return free_slot;
      }
      write_inode->inode.data[free_slot] = block_id;
      writeblock(write_inode, write_inode_id);
      free(write_inode);
      //an empty block is written to disk, to ensure it can be read safely
      union lfs_block* new_block = calloc(1, LFS_BLOCK_SIZE);
      if(!new_block)
      {
        free(write_inode);
        return -ENOMEM;
      }
      writeblock(new_block, block_id);
      free(new_block);
    }
    //read in the inode, to update the blocks used, and the size;
    write_inode = readblock(write_inode_id);
    if(write_inode < 0)
    {
      return write_inode;
    }
    write_inode->inode.blocks += new_blocks;
    writeblock(write_inode, write_inode_id);
    free(write_inode);
  }

  //find the block to start writing from
  int block_offset = offset/LFS_BLOCK_SIZE;
  //calculate the number of block to write to
  int num_blocks = (int) ceil((double) size/ (double) LFS_BLOCK_SIZE);
  //read in the inode, to write data to
  write_inode = readblock(write_inode_id);

  if(offset + size > write_inode->inode.size)
  {
    //if the size to write is larger than the previous inode size, then update
    write_inode->inode.size = offset + size;
  }

  if(write_inode < 0)
  {
    return write_inode;
  }
  for(int i = block_offset + 1; i < num_blocks + block_offset + 1; i++) //add one to offset the name data block
  {
    union lfs_block* data_block = readblock(write_inode->inode.data[i]);
    if(data_block < 0)
    {
      return data_block;
    }
    memcpy(&data_block->data[offset % LFS_BLOCK_SIZE], &buf[offset - org_offset], LFS_BLOCK_SIZE - (offset % LFS_BLOCK_SIZE) );
    writeblock(data_block->data, write_inode->inode.data[i]);
    if(size < LFS_BLOCK_SIZE){
      //less than an entire block has been written,-
      offset += size;
      size -= size;
    }
    else
    {
      //an entire block has been wriiten
      offset += (long) LFS_BLOCK_SIZE;
      size -= (long) LFS_BLOCK_SIZE;
     }
  }
  //update both access and modification times
  clock_gettime(CLOCK_REALTIME, &write_inode->inode.a_time);
  clock_gettime(CLOCK_REALTIME, &write_inode->inode.m_time);
  writeblock(write_inode, write_inode_id);
  //update parents, with the change in blocks used, and size
  set_num_blocks(write_inode_id, write_inode->inode.blocks - old_blocks, offset - old_size);
  printf("WRITE returning: %ld\n", offset - old_size);
  free(write_inode);
  return (offset - old_size); //return number of bytes written
}

int main( int argc, char *argv[] )
{
  file_system = fopen("file", "r+");
  //check if file exists
  if(file_system == NULL)
  {
    printf("%s\n", "Filesystem must exist, make an empty file with the name 'file' ");
    return -ENOENT;
  }
  //check if fs is being open for the first time
  union lfs_block* root = readblock(5);
  if(root->inode.blocks > 1  && root->inode.blocks < 20480 )
  {
    fuse_main( argc, argv, &lfs_oper );
    return 0;
  }
  //first time being opened
  setup();
  fuse_main( argc, argv, &lfs_oper );
	return 0;
}
