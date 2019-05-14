#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

FILE * file_system;

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, struct fuse_file_info *fi);

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = NULL,
	.mkdir = NULL,
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

unsigned int get_name(unsigned int block, char* name)
{
  printf("%s: %d\n", "reading search_block", block);
  //read the block to search for names
  union lfs_block* search_block = readblock(block);
  //read the address of the first data block

  int i = 1;
  unsigned int cand_block = search_block->inode.data[i];
  while(cand_block != 0)
  {
    printf("cand block: %d\n", cand_block);
    unsigned int cand_block = search_block->inode.data[i];
    //read the first data block
    union lfs_block* temp_block = readblock(cand_block);
    //read this blocks name block
    union lfs_block* name_block = readblock(search_block->inode.data[0]);

    //copy the name into array to compare
    char data[search_block->inode.name_length];
    memcpy(&data, &name_block->data, temp_block->inode.name_length+1);

    //if the names and data match, then the candidate block is correct
    if(strcmp(data, name) == 0)
    {
      return cand_block;
    }
    i += 1;
    //Reached end of array of block arrays
    if(i > 235)
    {
      return -ENOENT;
    }
  }
}

unsigned int get_block_from_path(const char* path)
{
  const char s[2] = "/";
  char *token;
  unsigned int block;
  printf("%s\n", "calling get_block_from_path");
  if(strcmp(path, "/") == 0) {printf("%s\n", "returning /");  return 5;};

  printf("%s\n", "tokenizing");
  token = strtok(path, s);
  block = get_name(5, token); //gets the block of the token in the root block
  while( token != NULL ) {

    printf( " %s\n", token );
    token = strtok(NULL, s);
    if(token == NULL){ printf("%s\n", "breaking");  break;}
    block = get_name(block, token);
  }

  return block;
}

int lfs_getattr( const char *path, struct stat *stbuf )
{
	printf("getattr: (path=%s)\n", path);
	memset(stbuf, 0, sizeof(struct stat));

   int num = get_block_from_path(path);
  union lfs_block* block = readblock(num);
  printf("%s %d\n", "gotblock", num);

  if(block->inode.type == 1)
  {
    stbuf->st_mode = S_IFDIR | 0777;
  }
  else
  {
    stbuf->st_mode = S_IFREG | 0777;
  }
  stbuf->st_size = block->inode.size;
  stbuf->st_atim = block->inode.a_time;
  stbuf->st_mtim = block->inode.m_time;

	return 0;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  printf("readdir: (path=%s)\n", path);
  int block = get_block_from_path(path);
  union lfs_block* dir_block = readblock(block);

  for (size_t i = 1; i < 236; i++) {
    // printf("%s\n", "reading tempblock");
    union lfs_block* temp_block = readblock(dir_block->inode.data[i]);
    //read this blocks name block
    // printf("%s\n", "reading nameblock");
    union lfs_block* name_block = readblock(temp_block->inode.data[0]);

    //copy the name into array to compare
    // printf("%s\n", "creating block");
    char data[temp_block->inode.name_length];
    // printf("%s\n", "cpy data");
    memcpy(&data, &name_block->data, temp_block->inode.name_length+1);
    printf("%s\n", data);
    if(strcmp(data, "") == 0)
    {

    }
    else{
      // printf("%s\n", "filling");
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
