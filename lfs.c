#include <math.h>
#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

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


static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = NULL,
	.mkdir = lfs_mkdir,
	.unlink = NULL,
  .create = lfs_create,
	.rmdir = lfs_rmdir,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = lfs_write,
	.rename = NULL,
	.utime = NULL
};

typedef struct lfs_inode //sizeof() = 512
{
  struct timespec a_time; //set with clock_gettime(CLOCK_REALTIME, &timespace a_time)
  struct timespec m_time; //16 bytes

  //Parents are referenced by their block num of their inode, as a 2 bytes integer (within the range of the num of blocks)
  unsigned short parent;  // id of the parent //2 bytes
  //chilren of folders are contained the in the data block
  unsigned char type;     //1 byte
  unsigned int size;  //4 bytes
  int name_length;
  unsigned short data[234]; //array of block ids can hold 234*512 = 0.24 MB
} inode_t;

typedef union lfs_block  //with union block can either be data or inode_t
{
  inode_t inode;
  unsigned char data[512];

}lfs_block ;

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
	int offset = 512*block;
	fseek(file_system, offset, SEEK_SET);
	if(fwrite(buf, 512, 1, file_system) != 1)
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
  void* buffer = calloc(1, sizeof(char) * 512);
  if(!buffer)
  {
    return (void*) -ENOMEM;
  }

	int offset = 512*block;
  if(fseek(file_system, offset, SEEK_SET) < 0){
    free(buffer);
    return (void*) -EAGAIN;
  }
  if(fread(buffer, 512, 1, file_system) != 1)
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
    for (size_t i = 0; i < 512; i++)			//THIS CAN BE DONE WITH WHILE?
    {
      if(bitmap_block->data[i] < 255)
      {
         free_bank = i;
        break;
      }
     }
    if(free_bank != -1)
    {
       break;
    }
  }
  //the right bank has been obtained, now find the correct bit
  unsigned char temp_byte = 0;
  memcpy(&temp_byte, &bitmap_block->data[free_bank], sizeof(char));

  int bit = 0;
  for (size_t i = 0; i < 8; i++) {
    bit = (temp_byte >> i) & 1U;
     if(bit == 0)
     {
      bit = i;
      temp_byte |= 1 << bit;
      break;
    }
  }
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

  //Get block for the name of root node
	disk_block->inode.data[0] = get_block(); //get first block
	union lfs_block *name = calloc(1, sizeof(lfs_block));

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

//search for directory or file with name in directory at block
int get_name(unsigned int block, char* name)
{
   //read the block to search for names
  union lfs_block* search_block = readblock(block);
  //read the address of the first data block

  int i;
  int cand_block;
  for (i = 1; i < 234; i++)
  {
    cand_block = search_block->inode.data[i];
    if(cand_block > 0)
    {
      union lfs_block* temp_block = readblock(cand_block);
      //read this blocks name block
      union lfs_block* name_block = readblock(temp_block->inode.data[0]);

      //copy the name into array to compare
      char data[temp_block->inode.name_length];
      memcpy(&data, &name_block->data, temp_block->inode.name_length);
      //if the names and data match, then the candidate block is correct
      if(strcmp(data, name) == 0)
      {
        return cand_block;
      }
    }
  }
  return -ENOENT;
}

//get a free slot for a block pointer in an inode
unsigned short get_free_slot_dir(union lfs_block* block)
{
  int free_slot = 0;
  while(block->inode.data[free_slot] != 0 && block->inode.data[free_slot] < 20480)
  {
    printf("FREE SLOT: content of freeslot %d: %d\n",free_slot, block->inode.data[free_slot]);
    free_slot += 1;
  }
  printf("FREE SLOT: returning %d\n", free_slot);
  return free_slot;
}

int get_block_from_path(const char* path)
{
  const char s[2] = "/";
  char *path_element;
  int block;
  if(strcmp(path, "/") == 0) {printf("%s\n", "GET_BLOCK_PATH: Returning root node");  return 5;};

  path_element = strtok((char *) path, s);
  block = get_name(5, path_element); //gets the block of the path_element in the root block
  if(block < 6)
  {
    return -ENOENT;
  }
  while( path_element != NULL ) {
    path_element = strtok(NULL, s);
    if(path_element == NULL)
    {
      printf("%s\n", "GET_BLOCK_PATH: Got the last possible element");
      break;
    }
    block = get_name(block, path_element);
  }

  union lfs_block* check_block = readblock(block);
  union lfs_block* nameblock = readblock(check_block->inode.data[0]);
  char data[512];
  memcpy(&data, &nameblock->data, check_block->inode.name_length);
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

int lfs_getattr( const char *path, struct stat *stbuf )
{
	printf("getattr: (path=%s)\n", path);
	memset(stbuf, 0, sizeof(struct stat));

  int num = get_block_from_path(path);
  if(num < 0)
  {
    return num;
  }

  union lfs_block* block = readblock(num);

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
  stbuf->st_atim = block->inode.a_time;
  stbuf->st_mtim = block->inode.m_time;


	return 0;
}

int lfs_mkdir(const char* path, mode_t mode)
{
  //get a new block for the inode
  int block = get_block();
  //get block of parent dir
  char temp[512];
  strcpy(temp, path);
  unsigned short parent_block_id = get_block_from_path(dirname(temp));
  union lfs_block* parent_block = readblock(parent_block_id);

  int free_slot = get_free_slot_dir(parent_block);
  //insert reference to the new dir into parent dir
  parent_block->inode.data[free_slot] = block;
  //Write the new parent to disk
  writeblock(parent_block, parent_block_id);

  //setup the new inode for the dir
  union lfs_block* new_dir = calloc(1, sizeof(lfs_block));
  new_dir->inode.parent = parent_block_id;
  new_dir->inode.type = 1;
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.m_time);

  //create block for the name
  new_dir->inode.data[0] = get_block();
  union lfs_block* name_block = calloc(1,sizeof(lfs_block));

  new_dir->inode.name_length = strlen(basename((char *) path)) + 1;
  memcpy(name_block->data, basename((char *) path), new_dir->inode.name_length);

  writeblock(name_block, new_dir->inode.data[0]);
  writeblock(new_dir, block);

  free(new_dir);
  free(parent_block);
  free(name_block);

  return 0;
}

int lfs_rmdir(const char* path)
{
  int block = get_block_from_path(path);
  if(block < 0)
  {
    return block;
  }
  union lfs_block* rm_block = readblock(block);

  union lfs_block* rm_block_parent = readblock(rm_block->inode.parent);
   for(int i = 0; i < 234; i++)
  {
    if(rm_block_parent->inode.data[i] == block)
    {
      rm_block_parent->inode.data[i] = 0;
      break;
    }
  }
  writeblock(rm_block_parent,rm_block->inode.parent);

  free_block(rm_block->inode.data[0]);
  free_block(block);
  free(rm_block);
  free(rm_block_parent);

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

  for (int i = 1; i < 234; i++) {
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
  char temp[512];
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
  union lfs_block* new_file = calloc(1, sizeof(lfs_block));
  //allocate data block, for name
  union lfs_block* new_name = calloc(1, sizeof(lfs_block));
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
  new_file->inode.parent = parent_dir_id;
  clock_gettime(CLOCK_REALTIME, &new_file->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_file->inode.m_time);

  writeblock(new_file, new_file_id);
  fi->fh = new_file_id;
  free(new_file);


  return 0;
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
  int page_offset = offset/512;
  printf("READ: start reading from page: %d\n", page_offset);
  int retsize;

  //read in block of inode to read from
  unsigned int read_block_id = get_block_from_path(path);
  union lfs_block* read_block = readblock(read_block_id);
  printf("READ: size of file %d\n", read_block->inode.size);

  if(size > read_block->inode.size)
  {
    retsize = size;
    size = read_block->inode.size;
  }

  int num_pages = (int) ceil((double) size/ (double) 512); //number of pages to read
  printf("READ: total number of pages to read: %d\n", num_pages);

  for(int i = page_offset + 1; i < num_pages + page_offset + 1; i++)
  {
    union lfs_block* data_page = readblock(read_block->inode.data[i]);
    memcpy(&buf[offset - org_offset], &data_page->data[offset % 512], 512 - (offset % 512) );
    if(size - offset > 512){
      offset += (long) 512 - (offset % (long) 512);
    }
    else
    {
      offset += (long) size - ((long) 512 - (offset % (long) 512));
    }
  }
  printf("READ: offset %ld bytes\n",  offset);
  return retsize;
}

int lfs_write( const char *path, const char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{

  long org_offset = offset;
  int block = get_block_from_path(path);
  printf("WRITE: block of inode: %d\n", block);
  if(block < 0)
  {
    return -ENOENT;
  }

  union lfs_block* write_block = readblock(block);
  printf("%s\n", "WRITE: Free SLOTS:");
  get_free_slot_dir(write_block);
  printf("\n");
  if(write_block->inode.size < offset + size )
  {
    union lfs_block* new_page;
     int new_pages = (int)ceil((double)(offset + size - write_block->inode.size)/(double)512);

    for(int i = 0; i < new_pages; i++)
    {
      write_block = readblock(block);
      write_block->inode.size = write_block->inode.size + offset + size;
      int page_id = get_block();
       //update parent
      unsigned short slot = get_free_slot_dir(write_block);
      printf("WRITE: block of new data: %d\n", page_id);
      printf("WRITE: new slot for data block in inode: %d\n", slot);
      write_block->inode.data[slot] = page_id;
      writeblock(write_block, block);

      new_page = calloc(1, sizeof(lfs_block));
      writeblock(new_page, page_id);
    }
    free(new_page);
  }

  //we start writing from offset
  int page = offset/512;
  //read in page, copy from buf until

  int num_pages = (int) ceil((double) size/ (double) 512); //number of pages to write

 for(int i = page + 1; i < num_pages + page + 1; i++) //add one to offset the name data block
  {
    union lfs_block* data_page = readblock(write_block->inode.data[i]);
    memcpy(&data_page->data[offset % 512], &buf[offset - org_offset], 512 - (offset % 512) );
    writeblock(data_page->data, write_block->inode.data[i]);
    if(size - offset > 512){
      offset += (long) 512 - (offset % (long) 512);
    }
    else
    {
      offset += (long) size - ((long) 512 - (offset % (long) 512));
    }
  }
  return offset - org_offset;
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
