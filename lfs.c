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
int lfs_release(const char *path, struct fuse_file_info *fi);
int lfs_mkdir(const char* path, mode_t mode);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = NULL,
	.mkdir = lfs_mkdir,
	.unlink = NULL,
	.rmdir = NULL,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = NULL,
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
  unsigned int name_length;
  unsigned short data[236]; //array of block ids can hold 236*512 = 0.24 MB
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
void* readblock(int block)
// reads `size` bytes from disk and returns a void* pointer to the data
{
  if (block < 0)
	{
		return -EINVAL;
	}
  void* buffer = malloc(sizeof(char) * 512);
  if(!buffer)
  {
    return -ENOMEM;
  }

	int offset = 512*block;
  if(fseek(file_system, offset, SEEK_SET) < 0){
    free(buffer);
    return -EAGAIN;
  }
  if(fread(buffer, 512, 1, file_system) != 1)
  {
    free(buffer);
    return -EAGAIN;
  }
	return buffer;
}
int init_bitmap()
{
  union lfs_block* bitmap_block = malloc(sizeof(union lfs_block));
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
	union lfs_block* disk_block = malloc(sizeof(union lfs_block));

	disk_block->inode.parent = 0;
	disk_block->inode.type = 1;

  //Get block for the name of root node
	disk_block->inode.data[0] = get_block(); //get first block
	union lfs_block *name = malloc(sizeof(lfs_block));

  //Insert name length into inode
  disk_block->inode.name_length = strlen("/");
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
  int cand_block = search_block->inode.data[i];
  for (i = 1; i < 235; i++)
  {
    cand_block = search_block->inode.data[i];
    if(cand_block != 0)
    {
      printf("%s %d\n", "found block", i);
      union lfs_block* temp_block = readblock(cand_block);
      //read this blocks name block
      union lfs_block* name_block = readblock(temp_block->inode.data[0]);

      //copy the name into array to compare
      char data[temp_block->inode.name_length];
      memcpy(&data, &name_block->data, temp_block->inode.name_length);
      printf("name length %d\n", temp_block->inode.name_length);
      //if the names and data match, then the candidate block is correct
      printf("data %s, name %s\n", data, name);
      if(strcmp(data, name) == 0)
      {
        return cand_block;
      }
    }
  }
  return -ENOENT;
}

int get_block_from_path(const char* path)
{
  const char s[2] = "/";
  char *token;
  int block;
  if(strcmp(path, "/") == 0) {printf("%s\n", "returning /");  return 5;};
  printf("%s\n", "is not root");

  token = strtok(path, s);
  block = get_name(5, token); //gets the block of the token in the root block
  printf("get_name return: %d\n", block);
  if(block < 0)
  {
    printf("%s\n","block could not be found");
    return -ENOENT;
  }
  while( token != NULL ) {
    token = strtok(NULL, s);
    if(token == NULL){printf("%s\n", "no more");  break;}
     block = get_name(block, token);
  }
  if(block < 6) //values below 6 are system block
  {
    return -ENOENT;
  }

  union lfs_block* check_block = readblock(block);
  union lfs_block* nameblock = readblock(check_block->inode.data[0]);
   char data[512];
  memcpy(&data, &nameblock->data, check_block->inode.name_length);
  printf("%s\n", data);
  if(strcmp(data, basename(path)) == 0)
  {
    //block is one we are looking for
    return block;
  }
  //could not find such file or dir
   return -ENOENT;


}

int lfs_getattr( const char *path, struct stat *stbuf )
{
	printf("getattr: (path=%s)\n", path);
	memset(stbuf, 0, sizeof(struct stat));

  int num = get_block_from_path(path);
  if(num < 0)
  {
    printf("returning %d\n", num);
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
  printf("parent id %d\n", parent_block_id);
  union lfs_block* parent_block = readblock(parent_block_id);

  int free_slot = 1;
  while(parent_block->inode.data[free_slot] != 0)
  {
    printf("%d\n", parent_block->inode.data[free_slot]);
    free_slot += 1;
  }
  printf("free slot in parent %d\n", free_slot);
  //insert reference to the new dir into parent dir
  parent_block->inode.data[free_slot] = block;
  //Write the new parent to disk
  writeblock(parent_block, parent_block_id);

  //setup the new inode for the dir
  union lfs_block* new_dir = malloc(sizeof(lfs_block));
  new_dir->inode.parent = parent_block_id;
  new_dir->inode.type = 1;
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.a_time);
	clock_gettime(CLOCK_REALTIME, &new_dir->inode.m_time);

  //create block for the name
  new_dir->inode.data[0] = get_block();
  union lfs_block* name_block = malloc(sizeof(lfs_block));
  printf("path before basename %s\n", path);
  printf("path after basename %s\n", path);

  new_dir->inode.name_length = strlen(basename(path)) + 1;
  memcpy(name_block->data, basename(path), new_dir->inode.name_length);

  writeblock(name_block, new_dir->inode.data[0]);
  writeblock(new_dir, block);

  free(new_dir);
  free(parent_block);
  free(name_block);

  return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  printf("readdir: (path=%s)\n", path);
  int block = get_block_from_path(path);
  if(block < 0)
  {
    return -block; //return error given by get_block_from_path
  }
  union lfs_block* dir_block = readblock(block);
  if(!dir_block->inode.type == 1)
  {
    -ENOTDIR;
  }

  for (size_t i = 1; i < 236; i++) {
    if(dir_block->inode.data[i] > 0 && dir_block->inode.data[i] < 20480) //Check if numbers are valid
    {
      printf("found on %d, val %d\n", i, dir_block->inode.data[i]);
      printf("\n");
      union lfs_block* temp_block = readblock(dir_block->inode.data[i]);
      //read this blocks name block
      // printf("%s\n", "reading nameblock");
      union lfs_block* name_block = readblock(temp_block->inode.data[0]);

      //copy the name into array to compare
      // printf("%s\n", "creating block");
      char data[temp_block->inode.name_length];
      // printf("%s\n", "cpy data");
      printf("%d\n", temp_block->inode.name_length);
      memcpy(&data, &name_block->data, temp_block->inode.name_length);
      printf("%s\n", data);
      filler(buf, data, NULL, 0);
    }
  }


	return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	memcpy( buf, "Hello\n", 6 );
	return 6;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}


int main( int argc, char *argv[] )
{
  file_system = fopen("file", "r+");
  setup();
  //
  // union lfs_block* k = readblock(5);
  //
  // printf("name length: %d\n", k->inode.name_length);
  // printf("data block: %d, loading\n", k->inode.data[0]);
  //
  // union lfs_block* l = readblock(k->inode.data[0]);
  //
  // char data[k->inode.name_length];
  // memcpy(&data, &l->data, k->inode.name_length+1); //plus one to include null termination
  // printf("%s\n", data);


	fuse_main( argc, argv, &lfs_oper );

	return 0;
}
