#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int info[4];
  int counter;
  int pid;
  // child 1
  if (fork() == 0){
    sleep(1);
    set_cfs_priority(3);
    for(counter = 0; counter < 1000000000; counter++){
      if(counter % 100000000 == 0){
        sleep(10);
      }
    }
    pid = getpid();
    get_cfs_stats(pid, info);
    printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n", pid, info[0], info[1], info[2], info[3]);
    exit(0,"child 1 exited");
  }

  // child 2
  if(fork() == 0){
    sleep(2);
    set_cfs_priority(2);
    for(counter = 0; counter < 1000000000; counter++){
      if(counter % 100000000 == 0){
        sleep(10);
      }
    }
    pid = getpid();
    get_cfs_stats(pid, info);
    printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n", pid, info[0], info[1], info[2], info[3]);
    exit(0, "child 2 exited");
  }
    
    // Parent
  set_cfs_priority(0); 
  for (counter = 0; counter < 1000000000; counter++){
     if (counter % 100000000 == 0){
      sleep(10);
    }
  }

  pid = getpid();
  get_cfs_stats(pid, info);
  printf("pid: %d, cfs_priority: %d, rtime: %d, stime: %d, retime: %d\n", pid, info[0], info[1], info[2], info[3]);
  wait(0, 0);
  wait(0, 0);
  exit(0,"parent exited");
}
