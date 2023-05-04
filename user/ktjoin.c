
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void *ktdo(void){

  kthread_exit(kthread_id());
  return 0;
}

int main(){

 int i;
 int tids[NKT-1];
 int xstatus;
 int failure = 0;
 for(i = 0; i < NKT-1; i++){
   tids[i] = kthread_create(ktdo, malloc(4000), 4000);
 }

 for(i = 0; i < NKT-1; i++){
   kthread_join(tids[i], &xstatus);
   if(xstatus != tids[i]){
     printf("THREAD %d wrong exit status, expected %d got %d\n", tids[i], tids[i], xstatus);
     failure = 1;
   }
 }

 printf("%s\n", failure == 0 ? "SUCCESS" : "FAILURE");
 kthread_exit(0);
 return 0;
}

