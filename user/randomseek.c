#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"


static int strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

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

  for(i = 0; i < 255; i++){
    // printf("%d ", buf1[i]);
    if(buf1[i] != buf2[i])
      printf("random number mismatch at idx %d\n", i);
  }
  // printf("\n");

ret:
  close(random_fd);
  return fail;
}

int test_seek()
{
  int fd = open("seekfile", O_RDWR | O_CREATE | O_TRUNC);
  char buf[30] = {0};
  int fail = 0;
  write(fd, "hello world!", 13);
  seek(fd, 0, SEEK_SET);
  read(fd, buf, 13);
  seek(fd, -13, SEEK_CUR);
  read(fd, buf+14, 14);

  if(strncmp(buf, buf+14, 13) != 0){
    printf("SEEK TEST FAILED\n");
    fail = 1;
    goto ret;
  }

  buf[0] = 0;
  seek(fd, 900, SEEK_SET);
  seek(fd, -2, SEEK_CUR);
  read(fd, buf, 1);
  if(buf[0] != '!'){
    printf("buf[0] %d, ", buf[0]);
    printf("SEEK OUT OF BOUNDS 1 TEST FAILED\n");
    fail = 1;
    goto ret;
  }

  buf[0] = 0;
  seek(fd, 900, SEEK_CUR);
  seek(fd, -2, SEEK_CUR);
  read(fd, buf, 1);
  if(buf[0] != '!'){
    printf("SEEK OUT OF BOUNDS 2 TEST FAILED\n");
    fail = 1;
    goto ret;
  }

  buf[0] = 0;
  seek(fd, -100, SEEK_SET);
  read(fd, buf, 1);
  if(buf[0] != 'h'){
    printf("SEEK BELOW ZERO TEST FAILED\n");
    fail = 1;
    goto ret;
  }

  buf[0] = 0;
  seek(fd, -100, SEEK_CUR);
  read(fd, buf, 1);
  if(buf[0] != 'h'){
    printf("SEEK BELOW ZERO TEST FAILED\n");
    fail = 1;
    goto ret;
  }

  ret:
  close(fd);
  return fail;
}

int main()
{
  int success = 0;
  success |= test_random(0x2A);

  success |= test_random(0x01);
  
  success |= test_seek();

  printf(success ? "some tests failed\n" : "all tests passed\n");
  exit(success);
}

