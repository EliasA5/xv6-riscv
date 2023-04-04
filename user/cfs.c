#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N 1000000000
#define M 100000000
#define vruntime(p,rtime,stime,retime) (decay[p] * rtime/ (rtime+stime+retime + 1))
int
main(void)
{
  int info[4];
  int counter;
  int pid;
  int decay[3] = {75,100,125};
  // child 1
  if (fork() == 0){
    sleep(1);
    set_cfs_priority(3);
    for(counter = 0; counter < N; counter++){
      if(counter % M == 0){
        sleep(10);
      }
    }
    pid = getpid();
    get_cfs_stats(pid, info);
    printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d, vruntime: %d\n",\
            pid, info[0], info[1], info[2], info[3], vruntime(info[0], info[1], info[2], info[3]));
    exit(0,"child 1 exited");
  }

  // child 2
  if(fork() == 0){
    sleep(2);
    set_cfs_priority(2);
    for(counter = 0; counter < N; counter++){
      if(counter % M == 0){
        sleep(10);
      }
    }
    pid = getpid();
    get_cfs_stats(pid, info);
    printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d, vruntime: %d\n",\
            pid, info[0], info[1], info[2], info[3], vruntime(info[0], info[1], info[2], info[3]));
    exit(0, "child 2 exited");
  }
    
    // Parent
  set_cfs_priority(0); 
  for (counter = 0; counter < N; counter++){
     if (counter % M == 0){
      sleep(10);
    }
  }

  pid = getpid();
  get_cfs_stats(pid, info);
    printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d, vruntime: %d\n",\
            pid, info[0], info[1], info[2], info[3], vruntime(info[0], info[1], info[2], info[3]));
  wait(0, 0);
  wait(0, 0);
  exit(0,"parent exited");
}
