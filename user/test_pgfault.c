#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define ARRSIZE 18

int main()
{
  char *arr[ARRSIZE];
  int i;

  for(i = 0; i < ARRSIZE; i++){
    arr[i] = (char *) malloc(3000);
  }

  for(i = 0; i < ARRSIZE; i++){
    strcpy(arr[i], "hello world!");
  }

  for(i = 0; i < ARRSIZE; i++){
    printf("%d says: %s\n", i, arr[i]);
  }

  for(i = 0; i < ARRSIZE; i++){
    free(arr[i]);
  }

  return 0;
}

