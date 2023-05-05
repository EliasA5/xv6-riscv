
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void *ktdo(void){
  sleep(7);

  exit(kthread_id());
  kthread_exit(0);
  return 0;
}

void *child_program(void){
  int i;
  for(i = 0; i< NKT-1; i++){
    kthread_create(ktdo, malloc(4000), 4000);
  }
  ktdo();
  return 0;
}

int main(){
  int xstatus;
  int CID;
  if((CID = fork()) == 0){
    child_program();
  }

  if(wait(&xstatus) == -1){
    printf("wait for child process failed\nFAILURE\n");
    exit(-1);
  }
  
  printf("thread %d did the exit\nSUCCESS\n", xstatus);

  return 0;
}

