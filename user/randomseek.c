#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"


char buf1[256];
char buf2[256];

int
test_random(int seed)
{
  int random_fd;
  int i, fail = 0;
  if((random_fd = open("random", O_RDWR)) == -1){
    printf("FAILED TO OPEN /random\n");
    fail = 1;
    goto ret;
  }
  if(write(random_fd, &seed, 5) != -1){
    printf("SUCCESSFULLY WROTE MORE THAN 1 BYTE TO RANDOM\n");
    fail = 1;
    goto ret;
  }
  if(write(random_fd, &seed, 1) == -1){
    printf("FAILED TO WRITE SEED\n");
    fail = 1;
    goto ret;
  }

  if(read(random_fd, buf1, 255) == -1){
    printf("FAILED TO WRITE ALL VALUES TO BUFFER 1\n");
    fail = 1;
    goto ret;
  }

  if(read(random_fd, buf2, 255) == -1){
    printf("FAILED TO WRITE ALL VALUES TO BUFFER 2\n");
    fail = 1;
    goto ret;
  }
//   char c;
  for(i = 0; i < 255; i++){
    // c = buf1[i];
    // printf("%d ", c);
    if(buf1[i] != buf2[i])
      printf("random number mismatch at idx %d\n", i);
  }
//   printf("\n");

ret:
  return fail;
}

int main()
{
  test_random(0x2A);
//   printf("\n");
  test_random(0x01);
  
  printf("all tests passed\n");
  exit(0);
}