
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void *ktdo(void){
  char *args[] = {"./echo", malloc(30), 0};
  *args[1] = ((char) kthread_id()) + '0';
  args[2] = 0;
  strcpy(args[1]+1, " did the exec\nSUCCESS");
  sleep(7);
  exec(args[0], args);

  printf("returned from exec, FAILURE"); 
  return 0;
}

int main(){

  int i;
  for(i = 0; i< NKT-1; i++){
    kthread_create(ktdo, malloc(4000), 4000);
  }

  ktdo();
  return 0;
}
