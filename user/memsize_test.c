#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  printf("Memory size of the process before calling memsize: %d\n", memsize());
  char* tmp = malloc(20000);
  printf("Memory size of the process after calling memsize: %d\n", memsize());
  free(tmp); 
  printf("Memory size of the process after the release: %d\n", memsize());
  exit(0,0);
}
