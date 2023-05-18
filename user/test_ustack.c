#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/ustack.h"

#define ARRSIZE 10

int main()
{
  char *arr[ARRSIZE];
  int i, j;

  for(i = 0; i < ARRSIZE; i++){
    arr[i] = ustack_malloc(512-ARRSIZE+1+i);
    printf("iter %d: loc %d\n", i, arr[i]);
    for(j = 0; j < 512-ARRSIZE+1; j++)
      arr[i][j] = '0' + i;
    // printf("%s\n", arr[i]);
  }
  // print_stack();
  for(i = ARRSIZE+1; i; i--){
    printf("ustack_free size: %d\n", ustack_free());
  }

  return 0;
}

