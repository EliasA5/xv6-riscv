
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include <complex.h>


void *ktsleep(void){
  sleep(100);

  kthread_exit(0);
  return 0;
}

#define fibonum 1000000
void *ktwork(void){
  long int i,acc1,acc2,temp;
  for(i = 0, acc1 = acc2 = 1; i < fibonum; i++){
      temp = acc2;
      acc2 = acc1 + acc2;
      acc1 = temp;
  }

  printf("fibo%d = %d", fibonum, acc2);
  kthread_exit(0);
  return 0;
}

int main(){
  int xstatus;
  int tid;

  tid = kthread_create(ktsleep, malloc(4000), 4000);
  if (kthread_kill(tid) != 0) {
      printf("failed to kill sleeping thread\nFAILURE\n");
      exit(-1);
  }
  if (kthread_join(tid, &xstatus) != 0 || xstatus != -1){
    
      printf("failed to join sleeping thread, or wrong exit code\nFAILURE\n");
      exit(-1);
  }

  tid = kthread_create(ktwork, malloc(4000), 4000);
  if (kthread_kill(tid) != 0) {
      printf("failed to kill running thread\nFAILURE\n");
      exit(-1);
  }
  if (kthread_join(tid, &xstatus) != 0 || xstatus != -1){
    
      printf("failed to join running thread, or wrong exit code\nFAILURE\n");
      exit(-1);
  }

  printf("SUCCESS\n");
  kthread_exit(0);
  return 0;
}

