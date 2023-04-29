
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/uthread.h"


#define NUM_THREADS 4

void ultfun(void){
  int i = kthread_id();
  int j = uthread_id();
  
  printf("Hi. I'm kthread %d, uthread %d, yielding\n", i, j);
  uthread_yield();
  printf("Hi. I'm kthread %d, uthread %d, exiting\n", i, j);
  uthread_exit();
}

void *printme(void) {
  int j;
  int i = kthread_id();
  for(j = 0; j < 3; j++){
    uthread_create(ultfun, HIGH);
  }
  uthread_start_all();
  printf("uthread return after start_all", i);
  kthread_exit(-1);
  return 0;
}

void main() {

  int i;
  int tids[NUM_THREADS];
  void *stacks[NUM_THREADS];
  int retval;

  for (i = 0; i < NUM_THREADS; i++) {
    stacks[i] = malloc(STACK_SIZE); 
    tids[i] = kthread_create(printme, stacks[i], STACK_SIZE);
  }

  for (i = 0; i < NUM_THREADS; i++) {
    printf("Trying to join with tid%d\n", i);
    kthread_join(tids[i], &retval);
    printf("Joined with tid%d\n", i);
    free(stacks[i]);
  }
}
