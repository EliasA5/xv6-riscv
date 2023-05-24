#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  uint i;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
      for(i = 0; i < MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++)
        p->swap_metadata[i].offset = i*PGSIZE;
  }
}

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

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  uint i;
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  proc_freeswapmetadata(p);
#if SWAP_ALGO == SCFIFO
  p->curr_psyc_page = 0;
#elif SWAP_ALGO == NFUA
  for(i = 0; i < MAX_TOTAL_PAGES; i++)
    p->nfua_counters[i].value = 0;
#elif SWAP_ALGO == LAPA
  for(i = 0; i < MAX_TOTAL_PAGES; i++)
    p->lapa_counters[i].value |= ~p->lapa_counters[i].value;
#endif
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

void
proc_freeswapmetadata(struct proc *p)
{
  uint i;
  for(i = 0; i < MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++)
    p->swap_metadata[i].used = 0;
}

int
remove_swap_page(struct proc *p, pagetable_t pagetable, uint64 idx)
{
  // printf("rm_swp_page: idx %d, used %d\n", idx, p->swap_metadata[idx].used);
  if(p->pagetable == pagetable && p->swap_metadata[idx].used){
    p->swap_metadata[idx].used = 0;
    return 0;
  }
  return -1;
}

static int
add_swap_page(struct proc *p, pte_t *pte)
{
  uint i;
  if(p->swapFile == 0)
    return -1;
  for(i = 0; i < MAX_TOTAL_PAGES - MAX_PSYC_PAGES; i++){
    if(p->swap_metadata[i].used != 0)
      continue;
    p->swap_metadata[i].used = 1;
    writeToSwapFile(p, (char *) PTE2PA(*pte), p->swap_metadata[i].offset, PGSIZE);
    kfree((void *) PTE2PA(*pte));
    *pte = (i << PGSHIFT) | PTE_FLAGS(*pte);
    *pte &= ~PTE_V;
    *pte |= PTE_PG;
    // printf("add_swp_page: i %d, pte %x, used %d\n", *pte >> PGSHIFT, *pte, p->swap_metadata[i].used);
    return 0;
  }
  return -1;
}

static void
swap_out_pages(struct proc *p, uint64 va, uint npages)
{
  uint64 i, a;
  pte_t *pte;

  for(a = va, i = npages; i > 0; a += PGSIZE, i--){
    if((pte = walk(p->pagetable, a, 0)) == 0)
      panic("swap_out_pages: walk");

    if((*pte & PTE_V) == 0 && (*pte & PTE_PG) != 0)
      panic("swap_out: already swapped");
    if((*pte & PTE_V) == 0)
      panic("swap_out: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("swap_out: not a leaf");
    if((*pte & PTE_U) == 0)
      panic("swap_out: trying to swap out non-user page");
    if(add_swap_page(p, pte) != 0)
      panic("swap_out: couldn't swap pages");
  }

}


static int
swap_between_pages(struct proc *p, pte_t *pte_psyc, pte_t *pte_swap, uint64 va)
{
  char *mem;

  if((mem = (char *) kalloc()) == 0)
    return -1;
  if((readFromSwapFile(p, mem, p->swap_metadata[*pte_swap >> PGSHIFT].offset, PGSIZE)) == -1){
    printf("swap_between_pages: failed to read from swp file\n");
    kfree(mem);
    return -1;
  }
  p->swap_metadata[*pte_swap >> 12].used = 0;
  *pte_swap = PA2PTE((uint64) mem) | PTE_FLAGS(*pte_swap);
  *pte_swap &= ~(PTE_PG | PTE_A);
  *pte_swap |= PTE_V;
#if SWAP_ALGO == NFUA
    p->nfua_counters[PGROUNDDOWN(va)/PGSIZE].value = 0;
#elif SWAP_ALGO == LAPA
    p->lapa_counters[PGROUNDDOWN(va)/PGSIZE].value |= ~p->lapa_counters[PGROUNDDOWN(va)/PGSIZE].value;
#endif
  if(add_swap_page(p, pte_psyc) != 0){
    panic("swap_between_pages: couldn't swap out psyc page");
    return -1;
  }

  return 0;
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz, old_sz;
#if SWAP_ALGO != NONE
  uint64 start_addr;
  uint npages;
#endif
  struct proc *p = myproc();

  sz = old_sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
#if SWAP_ALGO != NONE
  if(PGROUNDUP(p->sz)/PGSIZE > MAX_TOTAL_PAGES){
    printf("pid %d exceeded maximum allowed pages, got %d\n", p->pid, PGROUNDUP(p->sz)/PGSIZE);
    exit(-1);
  }
  if(p->swapFile && PGROUNDUP(p->sz)/PGSIZE > MAX_PSYC_PAGES && p->sz > old_sz){
    if(PGROUNDUP(old_sz)/PGSIZE > MAX_PSYC_PAGES){
      npages = (PGROUNDUP(p->sz) - PGROUNDUP(old_sz)) / PGSIZE;
      start_addr = PGROUNDUP(old_sz);
    } else {
      npages = PGROUNDUP(p->sz)/PGSIZE - MAX_PSYC_PAGES;
      start_addr = MAX_PSYC_PAGES * PGSIZE;
    }
    swap_out_pages(p, start_addr, npages);
  }
#endif

  return 0;
}

#if SWAP_ALGO == SCFIFO
static pte_t *
scfifo(struct proc *p)
{
  pte_t *pte_psyc;
  for(;; p->curr_psyc_page += PGSIZE){
    if(p->curr_psyc_page > p->sz)
      p->curr_psyc_page = 0;
    if((pte_psyc = walk(p->pagetable, p->curr_psyc_page, 0)) == 0)
      continue;
    if((*pte_psyc & PTE_V) == 0)
      continue;
    if((*pte_psyc & PTE_U) == 0)
      continue;
    if((*pte_psyc & PTE_A) != 0){
      *pte_psyc &= ~PTE_A;
      continue;
    }
    // printf("name: %s, PGFAULT: va %p, curr_psy_va %p, curr_psy_pte %p\n",
    //         p->name, va, p->curr_psyc_page, *pte_psyc);
    break;
  }
  return pte_psyc;
}
#endif

#if (SWAP_ALGO == NFUA)
static void
nfua_tick(struct proc *p)
{
  pte_t *pte_psyc;
  uint i;
  for(i = 0; i < PGROUNDUP(p->sz)/PGSIZE; i++){
    if((pte_psyc = walk(p->pagetable, i*PGSIZE, 0)) == 0)
      continue;
    if((*pte_psyc & PTE_V) == 0)
      continue;
    if((*pte_psyc & PTE_U) == 0)
      continue;
    p->nfua_counters[i].value >>= 1;
    if((*pte_psyc & PTE_A) != 0)
      p->nfua_counters[i].value |= 1 << (sizeof(p->nfua_counters[i].value)*8-1);
  }
}

static pte_t *
nfua(struct proc *p)
{
  pte_t *pte_psyc;
  uint i;
  uint min_idx = 0, found;
  nfua_tick(p);
  for(i = 0; i < PGROUNDUP(p->sz)/PGSIZE; i++){
    if((pte_psyc = walk(p->pagetable, i*PGSIZE, 0)) == 0)
      continue;
    if((*pte_psyc & PTE_V) == 0)
      continue;
    if((*pte_psyc & PTE_U) == 0)
      continue;
    if(found == 0){
      found = 1;
      min_idx = i;
      continue;
    }
    else if(p->nfua_counters[i].value < p->nfua_counters[min_idx].value)
      min_idx = i;
  }

  pte_psyc = walk(p->pagetable, min_idx*PGSIZE, 0);

  return pte_psyc;
}
#endif

#if SWAP_ALGO == LAPA

static uint
popcount(uint32 a)
{
  uint i = 0;
  while (a){
    i = a & 1 ? i+1 : i;
    a >>= 1; 
  }
  return i;
}

static void
lapa_tick(struct proc *p)
{
pte_t *pte_psyc;
  uint i;
  for(i = 0; i < PGROUNDUP(p->sz)/PGSIZE; i++){
    if((pte_psyc = walk(p->pagetable, i*PGSIZE, 0)) == 0)
      continue;
    if((*pte_psyc & PTE_V) == 0)
      continue;
    if((*pte_psyc & PTE_U) == 0)
      continue;
    p->lapa_counters[i].value >>= 1;
    if((*pte_psyc & PTE_A) != 0)
      p->lapa_counters[i].value |= 1 << (sizeof(p->lapa_counters[i].value)*8-1);
  }
}

static pte_t *
lapa(struct proc *p)
{
  pte_t *pte_psyc;
  uint i;
  uint min_idx = 0, found;
  uint pop1, pop2;
  lapa_tick(p);
  for(i = 0; i < PGROUNDUP(p->sz)/PGSIZE; i++){
    if((pte_psyc = walk(p->pagetable, i*PGSIZE, 0)) == 0)
      continue;
    if((*pte_psyc & PTE_V) == 0)
      continue;
    if((*pte_psyc & PTE_U) == 0)
      continue;
    if(found == 0){
      found = 1;
      min_idx = i;
      continue;
    }
    else if((pop1 = popcount(p->lapa_counters[i].value)) < (pop2 = popcount(p->lapa_counters[min_idx].value)))
      min_idx = i;
    else if(pop1 == pop2 && p->lapa_counters[i].value < p->lapa_counters[min_idx].value)
      min_idx = i;
  }

  pte_psyc = walk(p->pagetable, min_idx*PGSIZE, 0);

  return pte_psyc;
}
#endif

int
handle_page_fault(struct proc *p, uint64 va)
{
  pte_t *pte, *pte_psyc;
#if SWAP_ALGO == NONE
  return -1;
#endif
  if(va >= MAXVA)
    return -1;
  if((pte = walk(p->pagetable, va, 0)) == 0)
    return -1;
  if((*pte & PTE_PG) == 0){
    printf("handle_page_fault: pte not paged 0x%x\n", *pte);
    return -1;
  }
#if SWAP_ALGO == SCFIFO
  pte_psyc = scfifo(p);
#elif SWAP_ALGO == NFUA
  pte_psyc = nfua(p);
#elif SWAP_ALGO == LAPA
  pte_psyc = lapa(p);
#else
#error no SWAP_ALGO chosen
#endif

  if(swap_between_pages(p, pte_psyc, pte, va) != 0)
    panic("PGFAULT: couln't swap\n");

  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  // Allocate a swap file
  if(strncmp(p->name, "init", 5) != 0 && strncmp(p->name, "sh", 3) != 0 && createSwapFile(np) != 0){
    acquire(&np->lock);
    freeproc(np);
    release(&np->lock);
    return 0;
  }

  // Copy parent swapfile into np swapfile
  if(p->swapFile && np->swapFile && copySwapFile(p, np) != 0){
      removeSwapFile(np);
      acquire(&np->lock);
      freeproc(np);
      release(&np->lock);
      return 0;
  }

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  // remove swap file, the function checks if the swap file exists
  removeSwapFile(p);
  p->swapFile = 0;

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
