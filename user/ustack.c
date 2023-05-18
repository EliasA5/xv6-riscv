#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "user/ustack.h"
#include "kernel/riscv.h"

// MAXUALLOC = 512

#define PGNUM(sz) ((sz) >> PGSHIFT)

struct header{
  struct header *ptr;
  uint16 size;
};

typedef struct header Header;

static Header *base;
static Header *head = 0;
static uint64 stack_size;

void *ustack_malloc(uint len)
{
  Header *p;
  uint page_diff;
  if(len > MAXUALLOC)
    return (void *) -1;

  len += sizeof(Header);
  if(head == 0){
    base = (Header *) sbrk(PGSIZE);
    base->ptr = base;
    base->size = stack_size = sizeof(Header);
    head = base;
  }
  if((page_diff = PGNUM(stack_size + len)) > PGNUM(stack_size))
    p = (Header *) sbrk(PGSIZE);
  else
    p = ((void *) (head)) + head->size;

  p->ptr = head;
  p->size = len;
  stack_size += len;
  // printf("p %d, prev %d, plen %d, len %d, ",
  //     p, p->ptr, p->size, len);
  head = p;
  return (void *) (p + 1);
}

int ustack_free(void)
{
  Header *p,*prev;
  uint old_size;
  uint page_diff;
  if(head == 0 || head == base)
    return -1;
  p = head;
  old_size = p->size;
  prev = p->ptr;
  // went down a page
  if((page_diff = PGNUM(stack_size - p->size)) < PGNUM(stack_size))
    sbrk(-PGSIZE); 
  else if(page_diff > PGNUM(stack_size)){
    printf("ustack_free underflow\n");
    return -1;
  }
  head = prev;
  stack_size -= old_size;

  return old_size - sizeof(Header);
}

