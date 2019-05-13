#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdint.h>
#include <limits.h>

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
  // unsigned char data[512];
  //
  //
  unsigned char ja;
  ja |= 1UL << 7;
  ja |= 1UL << 6;
  ja |= 1 << 5;
  ja |= 1 << 4;
  ja |= 1 << 3;
  ja |= 1 << 2;
  ja |= 1 << 1;
  ja |= 1 << 0;

  ja ^= 1UL << 6;
  ja ^= (1UL << 1);

  bin(ja);

  if(ja < 255){
    printf("%s\n", "ja");
  }


}
