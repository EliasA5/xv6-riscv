#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];


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

void kthreadinit(struct proc *p)
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

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

// TODO: delte this after you are done with task 2.2
void allocproc_help_function(struct proc *p) {
  p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

  p->context.sp = p->kthread->kstack + PGSIZE;
}