
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

// Linear feedback shift register
// Returns the next pseudo-random number
// The seed is updated with the returned value
static uint8
lfsr_char(uint8 lfsr) {
  uint8 bit;
  bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
  lfsr = (lfsr >> 1) | (bit << 7);
  return lfsr;
}

struct {
    struct spinlock lock;
    uint8 seed;
} rand;

int
randomwrite(int user_src, uint64 src, int n)
{
  char c;
  if(n != 1)
    return -1;
  if(either_copyin(&c, user_src, src, n) == -1)
    return -1;
  acquire(&rand.lock);
  rand.seed = (uint8) c;
  release(&rand.lock);
  return 1;
}

int
randomread(int user_dst, uint64 dst, int n)
{
  int w = 0;
  acquire(&rand.lock);
  for(; n > 0; n--, w++, dst++){
    rand.seed = lfsr_char(rand.seed);
    if(either_copyout(user_dst, dst, &rand.seed, 1) == -1)
        break; 
  }
  release(&rand.lock);
  return w;
}

void
randominit(void)
{
  initlock(&rand.lock, "rand");
  rand.seed = 0x2A;

  devsw[RANDOM].read = randomread;
  devsw[RANDOM].write = randomwrite;
}
