#include "dir.h"
#include "lfs.h"
#include "inode.h"

int writeblock(void* buf, int block_id, size_t size)
{
	if (size < 0 || size > 512)
	{
		return -EINVAL;
	}
	if (buf == NULL)
	{
		return -EINVAL;
	}
	if (block_id < 0)
	{
		return -EINVAL;
	}
	int offset = 512*block_id;
	fseek(file_system, offset, SEEK_SET);
	if(fwrite(buf, size, 1, file_system) != 1)
  {
		return -EAGAIN;
	}
	return size;
}

void* readblock(int block_id, size_t size)
// reads `size` bytes from disk and returns a void* pointer to the data
{
	if (size < 0 || size > 512)
	{
		return -EINVAL;
	}
  if (block_id < 0)
	{
		return -EINVAL;
	}
  void* buffer = malloc(size);
  if(!buffer)
  {
    return -ENOMEM;
  }

	int offset = 512*block_id;
  if(fseek(file_system, offset, SEEK_SET) < 0){
    free(buffer);
    return -EAGAIN;
  }
  if(fread(buffer, size, 1, file_system) != 1)
  {
    free(buffer);
    return -EAGAIN;
  }
	return buffer;
}

int delete_block()
{
	return 0;
}

struct volume_control *get_volume_control()
{
  struct volume_control *volume_table = readblock(0, VOLUME_CONTROL_SIZE);
  if(volume_table < 0)
	{
		return NULL;
	}

	return volume_table;
}

int get_free_block()
{
  printf("%s\n", "getting a free datablock");
  struct volume_control *table = readblock(0, VOLUME_CONTROL_SIZE);
  if (!table)
  {
    return -EFAULT;
  }
  if (table->free_block_count == 0)
  {
    free(table);
    return -ENOSPC;
  }
	int freeblock = 0;
  printf("%s\n", "reading block");
  struct disk_block *temp_block = readblock(3, DISK_BLOCK_SIZE);
  if (temp_block < 0)
  {
    free(temp_block);
    return -EFAULT;
  }
  printf("priting first char of temp \n");
  printf("%c\n", temp_block->data[46]);
  printf("%c\n", temp_block->data[47]);
  printf("%c\n", temp_block->data[48]);
  printf("%c\n", temp_block->data[49]);

  while (freeblock == 0)
  {
    printf("%s\n", "loop");
    for (int i = 0; i < 508; i++) {
      printf("%d\n", i);
      if (strcmp(temp_block->data[i], '0') == 0)
      {
        printf("%d\n", i);
        freeblock = i;
        break;
      }
    }
  }
  free(temp_block);
  table->free_block_count--;
  if (writeblock(table, 0, sizeof(struct volume_control)) < 0)
  {
    return -EFAULT;
  }

  return freeblock;
}

int init_byte_map(int block_id, int free_blocks)
{
  struct disk_block *node = readblock(block_id, DISK_BLOCK_SIZE);
  if (node < 0 )
  {
    return -EFAULT;
  }
  for(int i = 0; i < 508; i++)
  {
    node->data[i] = '0';
  }
  // 3 + 41 44
  printf("Free blocks %d\n", free_blocks);
  int pages = (int) ceil((double) free_blocks / (double)508);
  printf("Cast int is : %d\n", pages);

  int last_block = block_id + pages; // last memory block we need
  for(int i = block_id ; i < last_block + 1; i++)
  {
    printf("%s %d\n", "looping time:", i);
    if (i == last_block)
    {
      node->next_block = 0;
    }
    else
    {
      node->next_block = i+1;
    }
    printf("%s\n", "writing");
    writeblock(node, i, DISK_BLOCK_SIZE);
  }
  printf("After loop : %d\n", pages);
  // update allocated

  node = readblock(3, DISK_BLOCK_SIZE);

  for (int i = 0; i < last_block; i++) {
    node->data[i] = '1';
  }
  writeblock(node, 3, DISK_BLOCK_SIZE);


  return pages;
}

int init_volume(int nblocks, int nblock_size, int max_entries)
{
  printf("init_volume | start init \n");
	// printf("Doing init volume.\n");
  int blocks_used = 1; // for volume table

	struct volume_control *disk = malloc(sizeof(struct volume_control));
	if (!disk)
	{
		return -ENOMEM;
	}
	disk->blocks = nblocks-1;
	disk->block_size = nblock_size;
  disk->max_file_entries = max_entries;
  disk->free_block_count = disk->blocks; //exclude vol control block, dir head and init inode
  disk->inode_block = 2;

	// printf("about to init inode.\n");
  int res = 0;

  res = init_inode(2, max_entries);
  if (res < 0)
  {
    free(disk);
    return res;
  }
  else // append blocks used
  {
      blocks_used += res;
  }
	printf("getting id.\n");
  ino_t root_inode = 1;
  printf("%s\n", "initting head");
  res = init_head(1, root_inode);
  if (res < 0)
  {
    free(disk);
    return -EFAULT;
  }
  blocks_used += res;

	printf("init bytemap\n");
  res = init_byte_map(blocks_used, disk->free_block_count);
  if (res < 0)
  {
    free(disk);
    return -EFAULT;
  }
  else
  {
    blocks_used += res;
  }
  disk->free_block_count -= blocks_used;
  printf("%s\n", "writing vol control block");
	writeblock(disk, 0, sizeof(struct volume_control));
  printf("init_volume | end init \n");
	return 0;
}
int fs_getattr( const char *path, struct stat *stbuf ) {
  struct linkedlist_dir *root =  readblock(1, LINKEDLIST_SIZE);
	if (root < 0)
	{
		free(root);
		return -EFAULT;
	}
  printf("path: %s\n", path);
	struct linkedlist_dir *node = get_link(path);
  if(!node)
  {
    printf("%s\n", "no such file");
    return -ENOENT;
  }

  if(node->type == 0)
  {
    printf("%s\n", "fype is file");

    stbuf->st_mode = S_IFMT; //is file
  }
  else
  {
    printf("%s\n", "fype is dir");
    stbuf->st_mode = S_IFDIR; //is directory
  }

  struct inode_page *page = malloc(INODE_PAGE_SIZE);
  get_inode_page(page, node->file_inode);
  int inode_index = node->file_inode % 7; //get the correct index  of the inode on the current page
  free(node);

  stbuf->st_size = page->inodes[inode_index].size;
  stbuf->st_atim = page->inodes[inode_index].atime;
  stbuf->st_mtim = page->inodes[inode_index].mtime;

	return 0;
}

int fs_readdir( const char *path,
                void *buf,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi )
{
	(void) offset;
	(void) fi;
 	printf("readdir : (path=%s)\n", path);

  struct linkedlist_dir *root = readblock(2, LINKEDLIST_SIZE);

	printf("ino_t of root is: %ld \n", root->file_inode);
	struct linkedlist_dir *listlink = get_link(path);

	filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

	// while(listlink)
  // {
  //   char name_buf[220];
  //   strcpy(name_buf, listlink->name);
	//
  //   if(strcmp(strcat(dirname(name_buf),"/"), path) == 0)
  //   {
  //     filler(buf, basename(listlink->name), NULL, 0);
  //   }
  //   readblock(listlink, listlink->next, LINKEDLIST_SIZE);
  // }

	return 0;
}

//Permission
int fs_open( const char *path,
						 struct fuse_file_info *fi )
{

  // printf("open: (path=%s)\n", path);
	return 0;
}

int fs_read(const char *path,
            char *buf,
            size_t size,
            off_t offset,
            struct fuse_file_info *fi )
{
  //should return num of bytes read (the number requsted on success)
	return 0;
}


int fs_write( const char *path,
							const char *buf,
							size_t size,
							off_t offset,
							struct fuse_file_info *fi)
{
	if (size % 512)
	{

	}
  return 0;
}

int fs_release(const char *path, struct fuse_file_info *fi) {

	printf("release: (path=%s)\n", path);

	return 0;
}

int fs_mkdir(const char *path,
	 					 mode_t mode)
{
      printf("mkdir : path (%s)\n", path);
      printf("%s\n", "getting id");
     ino_t inode_id = get_inode_id();
     printf("%s %d\n", "got inode id is: ", inode_id);
     if (inode_id < 0)
     {
       return (int) inode_id;
     }

     int blockid = get_free_block();
     printf("block id %d\n", blockid);
     if (blockid < 0)
     {
       return blockid;
     }
    add_entry(2, path, inode_id, 1, blockid);
  return 0;
}
int fs_rmdir(const char *path, mode_t mode){
  return 0;
}

int fs_utime(const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
  return 0;
}

int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi){
  return 0;
}


int main( int argc, char *argv[] ) {
	// printf("Running lfs.\n");
  printf("main func\n");
  file_system = fopen("file","r+b");
  if(!file_system)
  {
    //the file must not exist, a new one is created
    file_system = fopen("file", "w");
    fclose(file_system);
    file_system = fopen("file", "r+b");
  }
	init_volume(20480, 512, 30);
	fuse_main( argc, argv, &lfs_oper ); // Mounts the file system, at the mountpoint given

	return 0;
}
