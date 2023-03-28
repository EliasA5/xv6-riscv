#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char const *argv[])
{
  int pol;
  char *pol_type[] = {"xv6 default", "priority", "CFS"};
  if(argc != 2)
    exit(-1,"Must input only 1 argument");
  pol = atoi(argv[1]);
  if(set_policy(pol) == 0){
    printf("set scheduler policy to %s\n", pol_type[pol]);
    exit(0,0);
  }
  exit(-1, "set_policy system call failed");
}
