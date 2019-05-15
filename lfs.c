#include <math.h>
#include <fuse.h>
 #include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#define INODE_BLOCK_IDS 231
#define LFS_BLOCK_SIZE 512

FILE * file_system;

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_write( const char *, const char *, size_t, off_t, struct fuse_file_info *);
int lfs_release(const char *path, struct fuse_file_info *fi);
int lfs_mkdir(const char* path, mode_t mode);
int lfs_rmdir(const char* path);
int lfs_create(const char* path, mode_t mode, struct fuse_file_info *fi);
int lfs_unlink(const char *path);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
 	.mkdir = lfs_mkdir,
	.unlink = lfs_unlink,
  .create = lfs_create,
	.rmdir = lfs_rmdir,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = lfs_write,
	.rename = NULL,
	.utime = NULL,
};

typedef struct lfs_inode //sizeof() = LFS_BLOCK_SIZE
{
  //test with ls -ul (lists access time) | test with ls -l (lists modification time)
  struct timespec a_time; //set with clock_gettime(CLOCK_REALTIME, &timespace a_time)
  struct timespec m_time; //16 bytes

  //Parents are referenced by their block num of their inode, as a 2 bytes integer (within the range of the num of blocks)
  unsigned short parent;  // id of the parent //2 bytes
  //chilren of folders are contained the in the data block
  unsigned char type;     //1 byte
  unsigned int size;  //4 bytes
  unsigned int blocks; //blocks allocated to this inode
  int name_length;
  unsigned short data[INODE_BLOCK_IDS]; //array of block ids can hold 232*LFS_BLOCK_SIZE = 0.24 MB
} inode_t;

typedef union lfs_block  //with union block can either be data or inode_t
{
  inode_t inode;
  unsigned char data[LFS_BLOCK_SIZE];

} lfs_block;

void print_unsigned_binary(unsigned int n)
{
  if (n > 1)
  print_unsigned_binary(n/2);
  printf("%d", n % 2);
}

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

  return 0;
}

unsigned short get_block()
{
  union lfs_block* bitmap_block;
  unsigned int free_bank = -1;
  int k;
  for (k = 0; k < 5; k++) {   					//THIS CAN BE DONE WITH WHILE?
    bitmap_block = readblock(k);
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
       break;
    }
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
  return k*4096 + free_bank*8 + bit;
}

int free_block(unsigned int block)
{
  //Floor division to find the correct page
  int page = block/4096;
  //Find the correct indice in the array
  int bank = (block % 4096)/8;
  //Read the bank, to be manipulated
  union lfs_block* bitmap_block = readblock(page);
  unsigned char temp_byte;
  memcpy(&temp_byte, &bitmap_block->data[bank], sizeof(char));
  //Toggle the bit
  temp_byte ^= (1UL << (block % 8));
  //Copy the bank back, and write it to the disk
  memcpy(&bitmap_block->data[bank], &temp_byte, sizeof(char));
  writeblock(bitmap_block, page);
  return 0;
}

//setup the root node
int setup()
{
  init_bitmap();
	union lfs_block* disk_block = calloc(1, sizeof(union lfs_block));

	disk_block->inode.parent = 0;
	disk_block->inode.type = 1;
  disk_block->inode.blocks = 2;

  //Get block for the name of root node
	disk_block->inode.data[0] = get_block(); //get first block
	union lfs_block *name = calloc(1, LFS_BLOCK_SIZE);

  //Insert name length into inode
  disk_block->inode.name_length = 2;
  //Copy name of root to data block
  memcpy(name,"/", sizeof(char)*disk_block->inode.name_length);

	writeblock(&name->data, disk_block->inode.data[0]);

  //Set times for access and modification
	clock_gettime(CLOCK_REALTIME, &disk_block->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &disk_block->inode.m_time);

  //Write the disk block
	writeblock(disk_block, 5);
	return 0;
}

//search for directory or file with "name" in directory at "search_block_id"
int get_name(unsigned int search_block_id, char* name)
{
   //read the block to search for names
  union lfs_block* search_block = readblock(search_block_id);

  int i;
  int cand_block;
  for (i = 1; i < INODE_BLOCK_IDS; i++)
  {
    //read the address of the i'th data block
    cand_block = search_block->inode.data[i];
    if(cand_block > 0 && cand_block < 20480) //sanity check the candidate
    {
      printf("GET_NAME: cand_block %d, inode_num %d\n", search_block->inode.data[i], i);
      //inode of the candidate is read
      union lfs_block* cand_inode = readblock(cand_block);
      //read the candidate's name block
      union lfs_block* name_block = readblock(cand_inode->inode.data[0]);

      //copy the name into array to compare
      char data[cand_inode->inode.name_length];
      memcpy(&data, &name_block->data, cand_inode->inode.name_length);
      printf("GET_NAME: cand_block name: %s, pathname: %s\n", data, name);
      //if the names and data match, then the candidate block is correct
      if(strcmp(data, name) == 0)
      {
        printf("GET_NAME: names matched, returning %d\n", cand_block);
        return cand_block;
      }
      printf("GET_NAME: names did not match\n");
    }
  }
  //did not find the name in the dir
  return -ENOENT;
}

int get_block_from_path(const char* path)
{
  char path_save[LFS_BLOCK_SIZE];
  strcpy(path_save, path); //save the path, as strtok() modifies path

  char *path_element;
  int block;
  //If the path is root folder
  if(strcmp(path, "/") == 0)
  {
    printf("%s\n", "GET_BLOCK_PATH: Returning root node");
    return 5;
  }
   //get the path of the first folder or file
  path_element = strtok((char *) path, "/");
  //gets the block of the path_element in the root block
  block = get_name(5, path_element);
  if(block < 6)
  {
    //path does not exist
    return -ENOENT;
  }
  while( path_element != NULL )
  {
    //split next part of path
    path_element = strtok(NULL, "/");
    if(path_element == NULL)
    {
      //there is no next element, block contains the sought after block
      break;
    }
    block = get_name(block, path_element);
    if(block < 6)
    {
      //a block for name could not be found
      return -ENOENT;
    }
  }
  strcpy(path, path_save);
  //read in block, and its name
  union lfs_block* check_block = readblock(block);
  union lfs_block* nameblock = readblock(check_block->inode.data[0]);
  //read in the name, for comparison
  char data[check_block->inode.name_length];
  memcpy(&data, &nameblock->data, check_block->inode.name_length);
  printf("GET_BLOCK_PATH: name: %s, basename: %s\n", data, basename((char *) path));
  if(strcmp(data, basename((char *) path)) == 0)
  {
    //block is one we are looking for
    printf("GET_BLOCK_PATH: Found element: %s\n", basename((char *) path));
    return block;
  }
  //could not find such file or dir
  printf("GET_BLOCK_PATH: Could NOT FIND element: %s\n", basename((char *) path));
  return -ENOENT;
}

//get a free slot for a block pointer in an inode
unsigned short get_free_slot_dir(union lfs_block* block)
{
  int free_slot = 0;
  //block pointers must pass sanity check before being comfirmed as blocks
  while(block->inode.data[free_slot] != 0 && block->inode.data[free_slot] < 20480)
  {
    printf("FREE SLOT: content of freeslot %d: %d\n",free_slot, block->inode.data[free_slot]);
    free_slot += 1;
  }
  printf("FREE SLOT: returning %d\n", free_slot);
  return free_slot;
}

//updates of the number of blocks and bytes used by children
int set_num_blocks(const char* path, int dif_blocks, int dif_size)
{
  printf("SET NUM BLOCKS: %s, difference_blocks %d, difference_size %d\n", path, dif_blocks, dif_size);

  //base case
  char path_save[512];
  strcpy(path_save, path);
  if(strcmp(dirname(path_save), "/") == 0)
  {
    strcpy(path_save, path);
    union lfs_block* root_inode = readblock(5);
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
    strcpy(path_save, path);
    int child_inode_id = get_block_from_path(path_save);
    union lfs_block* child_inode = readblock(child_inode_id);
    union lfs_block* parent_inode = readblock(child_inode->inode.parent);

    printf("SET NUM BLOCKS: old: parent_inode->inode.blocks %d\n", parent_inode->inode.blocks);
    printf("SET NUM BLOCKS: old: parent_inode->inode.size %d\n",   parent_inode->inode.size);


    parent_inode->inode.blocks += dif_blocks;
    parent_inode->inode.size += dif_blocks;

    printf("SET NUM BLOCKS: new: parent_inode->inode.blocks %d\n", parent_inode->inode.blocks);
    printf("SET NUM BLOCKS: new: parent_inode->inode.size %d\n",   parent_inode->inode.size);

    writeblock(parent_inode, child_inode->inode.parent);
    free(parent_inode);
    free(child_inode);
    //recursivly call on parent
    strcpy(path_save, path);
    set_num_blocks(dirname(path_save), dif_blocks, dif_size);
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
  stbuf->st_size = block->inode.size;
  stbuf->st_blocks = block->inode.blocks;
  stbuf->st_atim = block->inode.a_time;
  stbuf->st_mtim = block->inode.m_time;
  stbuf->st_blksize = LFS_BLOCK_SIZE;

	return 0;
}

int lfs_mkdir(const char* path, mode_t mode)
{
  //get a new block for the inode
  int block = get_block();
  //save path, as it will be modified by path
  char save_path[LFS_BLOCK_SIZE];
  strcpy(save_path, path);
  //get block of parent dir and read it in
  unsigned short parent_block_id = get_block_from_path(dirname(save_path));
  union lfs_block* parent_block = readblock(parent_block_id);

  int free_slot = get_free_slot_dir(parent_block);
  //insert reference to the new dir into parent dir
  parent_block->inode.data[free_slot] = block;
  //Write the new parent to disk
  writeblock(parent_block, parent_block_id);
  free(parent_block);

  //setup the new inode for the dir
  union lfs_block* new_dir = calloc(1, LFS_BLOCK_SIZE);
  new_dir->inode.parent = parent_block_id;
  new_dir->inode.type = 1;
  new_dir->inode.blocks = 2;
  new_dir->inode.size = 0;
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.m_time);

  //create block for the name, and save it in the new inode
  new_dir->inode.data[0] = get_block();
  union lfs_block* name_block = calloc(1,LFS_BLOCK_SIZE);

  //set the length of the name, and copy the name into its block
  new_dir->inode.name_length = strlen(basename((char *) path)) + 1;
  memcpy(name_block->data, basename((char *) path), new_dir->inode.name_length);


  //write the name, new dir and free the calloc'ed memory
  writeblock(name_block, new_dir->inode.data[0]);
  free(name_block);
  writeblock(new_dir, block);
  free(new_dir);

  set_num_blocks(path, 2, 0); //

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
  if(rm_block->inode.type != 1)
  {
    return -ENOTDIR;
  }
  if(rm_block->inode.blocks > 2) //the two blocks are inode and name data
  {
    printf("number of blocks in folder %d\n", rm_block->inode.blocks);
    return -ENOTEMPTY;
  }

  union lfs_block* rm_block_parent = readblock(rm_block->inode.parent);
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

  //free the name block, and the inode of the directory, and free the memory allocated

  free_block(rm_block->inode.data[0]);
  free_block(rm_block_id);
  set_num_blocks(path, -2, 0);

  return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi )
{
	(void) offset;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  printf("readdir: (path=%s)\n", path);
  int block = get_block_from_path(path);
  if(block < 0)
  {
    return block; //return error given by get_block_from_path
  }
  union lfs_block* dir_block = readblock(block);
  if(!(dir_block->inode.type == 1))
  {
    return -ENOTDIR;
  }

  for (int i = 1; i < INODE_BLOCK_IDS; i++) {
    if(dir_block->inode.data[i] > 0 && dir_block->inode.data[i] < 20480) //Check if numbers are valid
    {
      printf("READDIR: found file or folder at %d, block %d\n", i, dir_block->inode.data[i]);

       union lfs_block* temp_block = readblock(dir_block->inode.data[i]);
      //read this blocks name block
      union lfs_block* name_block = readblock(temp_block->inode.data[0]);

      printf("READDIR: name is in block: %d\n", temp_block->inode.data[0]);
      //copy the name into array to compare
      char data[temp_block->inode.name_length];
      memcpy(&data, &name_block->data, temp_block->inode.name_length);
      printf("%s\n", data);
      filler(buf, data, NULL, 0);
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

  //read the parent dir block
  union lfs_block* parent_dir = readblock(parent_dir_id);
  int free_slot = get_free_slot_dir(parent_dir);
  //insert new file id, and write to disk
  parent_dir->inode.data[free_slot] = new_file_id;
  writeblock(parent_dir, parent_dir_id);
  free(parent_dir);

  //allocate inode, for new file
  union lfs_block* new_file = calloc(1, LFS_BLOCK_SIZE);
  //allocate data block, for name
  union lfs_block* new_name = calloc(1, LFS_BLOCK_SIZE);
  //insert length of name into inode
  new_file->inode.name_length = strlen(basename((char *) path)) + 1;

  unsigned short new_name_id = get_block();
  //copy the name into the data block and write
  memcpy(new_name->data, basename((char *) path), new_file->inode.name_length);
  writeblock(new_name, new_name_id);
  free(new_name);

  //set data, type, parent and clock
  new_file->inode.data[0] = new_name_id;
  //new_file->inode.data[1] = 0;

  new_file->inode.type = 0;
  new_file->inode.blocks = 2;
  new_file->inode.size = 0;
  new_file->inode.parent = parent_dir_id;
  clock_gettime(CLOCK_REALTIME, &new_file->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_file->inode.m_time);

  writeblock(new_file, new_file_id);
  fi->fh = new_file_id;
  free(new_file);


  set_num_blocks(path, 2, 0);

  return 0;
}

int lfs_unlink(const char *path)
{
  int rm_block_id = get_block_from_path(path);
  union lfs_block* rm_block = readblock(rm_block_id);

  set_num_blocks(path, -rm_block->inode.blocks, -rm_block->inode.size);

  union lfs_block* rm_block_parent = readblock(rm_block->inode.parent);

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
  writeblock(rm_block_parent, rm_block->inode.parent);
  free_block(rm_block_id);
  free(rm_block_parent);
  free(rm_block);
}


int lfs_open( const char *path, struct fuse_file_info *fi )
{
   //check if file exists
  int res = get_block_from_path(path);
  if(res > 0)
  {
    fi->fh = res;
  	return 0;
  }

  return -ENOENT;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi )
{

  int org_offset = offset;
  printf("READ: offset is: %d\n", org_offset);
  printf("READ: size  is: %ld\n", size);
  //we start read from offset
  int page_offset = offset/LFS_BLOCK_SIZE;
  printf("READ: start reading from page: %d\n", page_offset);
  int retsize;

  //read in block of inode to read from
  unsigned int read_inode_id = get_block_from_path(path);
  union lfs_block* read_inode = readblock(read_inode_id);
  printf("READ: size of file %d\n", read_inode->inode.size);

  if(size > read_inode->inode.size)
  {
    retsize = size;
    size = read_inode->inode.size;
  }

  int num_pages = (int) ceil((double) size/ (double) LFS_BLOCK_SIZE); //number of pages to read
  printf("READ: total number of pages to read: %d\n", num_pages);

  for(int i = page_offset + 1; i < num_pages + page_offset + 1; i++)
  {
    union lfs_block* data_page = readblock(read_inode->inode.data[i]);
    memcpy(&buf[offset - org_offset], &data_page->data[offset % LFS_BLOCK_SIZE], LFS_BLOCK_SIZE - (offset % LFS_BLOCK_SIZE) );
    if(size - offset > LFS_BLOCK_SIZE){
      offset += (long) LFS_BLOCK_SIZE - (offset % (long) LFS_BLOCK_SIZE);
    }
    else
    {
      offset += (long) size ;
    }
  }
  clock_gettime(CLOCK_REALTIME, &read_inode->inode.a_time);
  writeblock(read_inode, read_inode_id);

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

  int old_size = write_inode->inode.size;
  int old_blocks = write_inode->inode.blocks;

  if(write_inode->inode.size < offset + size )
  {
    union lfs_block* new_page;
    //number of new data blocks to be allocated
     int new_pages = (int)ceil((double)(offset + size - write_inode->inode.size)/(double)LFS_BLOCK_SIZE);

    for(int i = 0; i < new_pages; i++)
    {
      write_inode = readblock(write_inode_id);
      int page_id = get_block();
       //update parent
      unsigned short slot = get_free_slot_dir(write_inode);
      write_inode->inode.data[slot] = page_id;
      writeblock(write_inode, write_inode_id);

      new_page = calloc(1, LFS_BLOCK_SIZE);
      writeblock(new_page, page_id);
    }
    write_inode->inode.blocks += new_pages;
    write_inode->inode.size = offset + size;
    writeblock(write_inode, write_inode_id);
    free(new_page);
  }

  //we start writing from offset
  int page = offset/LFS_BLOCK_SIZE;
  //read in page, copy from buf until

  int num_pages = (int) ceil((double) size/ (double) LFS_BLOCK_SIZE); //number of pages to write
  write_inode = readblock(write_inode_id);
  for(int i = page + 1; i < num_pages + page + 1; i++) //add one to offset the name data block
  {
    union lfs_block* data_page = readblock(write_inode->inode.data[i]);
    memcpy(&data_page->data[offset % LFS_BLOCK_SIZE], &buf[offset - org_offset], LFS_BLOCK_SIZE - (offset % LFS_BLOCK_SIZE) );
    writeblock(data_page->data, write_inode->inode.data[i]);
    if(size < LFS_BLOCK_SIZE){
      //less than an entire block has been written
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
  clock_gettime(CLOCK_REALTIME, &write_inode->inode.a_time);
  clock_gettime(CLOCK_REALTIME, &write_inode->inode.m_time);
  writeblock(write_inode, write_inode_id);

  set_num_blocks(path, write_inode->inode.blocks - old_blocks, offset - old_size);
  printf("WRITE returning: %ld\n", offset - old_size);
  free(write_inode);
  return (offset - old_size);
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}


int main( int argc, char *argv[] )
{
  file_system = fopen("file", "r+");
  setup();

	fuse_main( argc, argv, &lfs_oper );

	return 0;
}
