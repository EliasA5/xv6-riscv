#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  write(1, "Hello World!\n", 13);
  exit(0);
}
