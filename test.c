 #include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdint.h>
#include <limits.h>
typedef struct lfs_inode //sizeof() = LFS_BLOCK_SIZE
{
  //test with ls -ul (lists access time) | test with ls -l (lists modification time)
  struct timespec a_time; //set with clock_gettime(CLOCK_REALTIME, &timespace a_time)
  struct timespec m_time; //16 bytes

  //Parents are referenced by their block num of their inode, as a 2 bytes integer (within the range of the num of blocks)
  short parent;  // id of the parent //2 bytes
  //chilren of folders are contained the in the data block
  unsigned char type;     //1 byte
  int size;  //4 bytes
  short blocks; //blocks allocated to this inode
  short name_length;
  unsigned short data[234]; //array of block ids can hold 232*LFS_BLOCK_SIZE = 0.24 MB
} inode_t;

void bin(unsigned n)
{
    /* step 1 */
    if (n > 1)
        bin(n/2);

    /* step 2 */
    printf("%d", n % 2);
}

//setting individual bits

int main(){
   printf("%d\n", sizeof(inode_t));

   return(0);


}
