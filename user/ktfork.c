
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void *ktdo(void){
  sleep(kthread_id());
  fprintf(2, "IM ALIIIIIIIVE (tid = %d)\n", kthread_id());

  kthread_exit(0);
  return 0;
}

int main() {
  int i;
  for (i = 0; i < NKT - 1; i++) {
    kthread_create(ktdo, malloc(4000), 4000);
  }
  sleep(5);
  printf("parent created NKT=%d threads, trying 1 more\n", NKT - 1);
  printf("%s\n", kthread_create(ktdo, malloc(4000), 4000) != -1 ? "FAILURE" : "SUCCESS");

  kthread_exit(0);
  return 0;
}
