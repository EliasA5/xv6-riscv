#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "user/ustack.h"
#include "kernel/riscv.h"

// MAXUALLOC = 512

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
  if(len > MAXUALLOC)
    return (void *) -1;

  len += sizeof(Header);
  if(head == 0){
    base = (Header *) sbrk(PGSIZE);
    base->ptr = base;
    base->size = stack_size = sizeof(Header);
    head = base;
  }
  // went up a page
  if(PGROUNDUP(stack_size) == PGROUNDDOWN(stack_size+len))
    sbrk(PGSIZE);

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
  if(head == 0 || head == base)
    return -1;
  p = head;
  old_size = p->size;
  prev = p->ptr;
  // went down a page
  if(PGROUNDDOWN(stack_size) == PGROUNDUP(stack_size - old_size))
    sbrk(-PGSIZE); 
  head = prev;
  stack_size -= old_size;

  // freed all allocated stacks
  if(head == base){
    head = 0;
    sbrk(-PGSIZE);
  }

  return old_size - sizeof(Header);
}

