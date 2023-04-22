#include "user/uthread.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct uthread THREADS[MAX_UTHREADS];
struct uthread *curr_thread;

void usched(void);
void usched1(void);

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
   struct uthread *t;
   int found = -1;
   for(t = THREADS; found != 0 && t < &THREADS[MAX_UTHREADS]; t++){
        if(t->state == FREE){
            found = 0;
            t->priority = priority;
            memset(&t->context, 0, sizeof(t->context));
            t->context.ra = (uint64) start_func;
            t->context.sp = (uint64) t->ustack + STACK_SIZE;
            t->state = RUNNABLE;
        } 
   } 
   return found;
}

void uthread_yield()
{
    curr_thread->state = RUNNABLE;
    usched();
}

void uthread_exit()
{
    curr_thread->state = FREE; 
    usched();
}

int uthread_start_all()
{
    static int started = 0;
    if(started != 0)
        return -1;
    started = 1;
    usched1();
    return -1;
}

enum sched_priority uthread_set_priority(enum sched_priority priority)
{
    enum sched_priority prev = curr_thread->priority;
    curr_thread->priority = priority;
    return prev;
}

enum sched_priority uthread_get_priority()
{
    return curr_thread->priority;
}

struct uthread* uthread_self()
{
    return curr_thread;
}

void usched()
{
    struct uthread *t;
    struct uthread *to_run;
    int found;
    found = 0;
    // search for a thread that has a high priority (if found)
    for (t = THREADS; t < &THREADS[MAX_UTHREADS]; t++) {
        if (t == curr_thread)
            continue;
        if (found == 0 && t->state == RUNNABLE) {
            found = 1;
            to_run = t;
        }
        if (t->state == RUNNABLE && t->priority > to_run->priority) {
            to_run = t;
        }
    }

    // select the current thread if it has a higher priority
    if (found == 1 && curr_thread->state == RUNNABLE &&
        to_run->priority < curr_thread->priority)
        to_run = curr_thread;

    // select the current thread if hasn't found any other threads
    if (found == 0 && curr_thread->state == RUNNABLE) {
        found = 1;
        to_run = curr_thread;
    }
    if (found == 0)
        exit(0);

    t = curr_thread;
    curr_thread = to_run;
    curr_thread->state = RUNNING;
    uswtch(&t->context, &curr_thread->context);
    
}

// First scheduling for a thread, does not return here
void usched1(void)
{
  struct uthread *t;
  struct uthread *dummy = {0};
  int found;
  found = 0;
  for (t = THREADS; t < &THREADS[MAX_UTHREADS]; t++) {
    if (found == 0 && t->state == RUNNABLE){
      found = 1;
      curr_thread = t;
    }
    if (t->state == RUNNABLE && t->priority > curr_thread->priority)
      curr_thread = t;
  }
  if (found == 0)
    exit(-1);
  curr_thread->state = RUNNING;
  uswtch(&dummy->context, &curr_thread->context);
}


