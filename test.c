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
   fopen("/tmp/mpoint/file.txt", "r+");

   return(0);


}
