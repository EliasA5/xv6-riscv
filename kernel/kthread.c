#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

extern void forkret(void);

struct spinlock kt_wait_lock;
// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

void
kthreadinit(struct proc *p)
{

  initlock(&p->tidlock, "thrd_counter");

  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&kt->lock, "thrd_lock");
    kt->state = T_UNUSED;
    kt->pp = p;
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

// Return the current struct kthread *, or zero if none.
struct kthread*
mykthread()
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *t = c->thread;
  pop_off();
  return t;
}

int
alloctid(struct proc *p)
{
  int tid;

  acquire(&p->tidlock);
  tid = p->tid_counter;
  p->tid_counter++;
  release(&p->tidlock);

  return tid;
}

void
freekthread(struct kthread *kt)
{

  kt->trapframe = 0;
  kt->tid = 0;
  kt->chan = 0;
  kt->killed = 0;
  kt->xstate = 0;
  kt->state = T_UNUSED;
}

struct trapframe*
get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

struct kthread*
allockthread(struct proc *p)
{
  struct kthread *t;
  //FIXME lock p->lock 
  for(t = p->kthread; t < &p->kthread[NKT]; t++) {
    acquire(&t->lock);
    if(t->state == T_UNUSED) {
      goto found;
    } else {
      release(&t->lock);
    }
  }
  return 0;

found:
  t->tid = alloctid(p);
  t->state = T_USED;
 
  
  // Allocate a trapframe page.
  t->trapframe = get_kthread_trapframe(p, t);

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&t->context, 0, sizeof(t->context));
  t->context.ra = (uint64)forkret;
  t->context.sp = t->kstack + PGSIZE;

  return t;
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;
  struct kthread *kt = mykthread();

  // Still holding t->lock from scheduler.
  release(&kt->lock);
  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

int
kthread_create(uint64 start_func, uint64 stack, uint stack_size)
{
  struct proc *p = myproc();
  struct kthread *kt;
  int tid;

  if((kt = allockthread(p)) == 0)
    return -1;

  tid = kt->tid;

  kt->trapframe->epc = start_func;
  kt->trapframe->sp = stack + stack_size;
  
  kt->state = T_RUNNABLE;

  release(&kt->lock);

  return tid;
}

int
ktkilled(struct kthread *kt)
{
  int k;

  acquire(&kt->lock);
  k = kt->killed;
  release(&kt->lock);

  return k;
}

int
kthread_kill(int tid)
{
  struct proc *p;
  struct kthread *kt;

  p = myproc();

  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->lock);
    if(kt->tid == tid){
      kt->killed = 1;
      if(kt->state == T_SLEEPING)
        kt->state = T_RUNNABLE;
      release(&kt->lock);
      return 0;
    }
    release(&kt->lock);
  }

  return -1;
} 

void
kthread_exit(int status)
{

  struct kthread *t;
  struct kthread *kt = mykthread();
  struct proc *p = myproc();

  acquire(&kt_wait_lock);

  // Some thread can be waiting on me to exit
  wakeup(kt);

  acquire(&kt->lock);

  kt->xstate = status;
  kt->state = T_ZOMBIE;

  release(&kt->lock);
  release(&kt_wait_lock);

  for(t = p->kthread; t < &p->kthread[NKT]; t++){
    acquire(&t->lock);
    if(t->state != T_UNUSED && t->state != T_ZOMBIE){
      // found another runnable thread
      release(&t->lock);
      acquire(&kt->lock);
      sched();
      panic("zombie thread exit");
    }
    release(&t->lock);
  }

  exit(status);
}

int
kthread_join(int tid, uint64 addr)
{
  return -1;
}



