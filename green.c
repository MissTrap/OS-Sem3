#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"

#define PERIOD 100
#define FALSE 0
#define TRUE 1
#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, NULL, FALSE};

static sigset_t block;  //used for sigpromask and timer.

static green_t *running = &main_green;
static void init() __attribute__((constructor));
/* stuff above is according to task description */

/* first element (thread) in ready queue */
struct green_t *flist = NULL;

/* used to insert a thread in a queue/list. */
void insert(struct green_t* thread, green_t** list) {
  if(thread != NULL) {
    green_t* finlist = *list;
    if(finlist != NULL)   { thread->next = finlist; }
    *list = thread;
  }
}

/* removes a specified thread from the ready queue */
void detatch(struct green_t *thread, green_t** list) {
  struct green_t *finlist = *list;
  if( finlist == thread )         {  *list = NULL; }
  else{
    while(finlist->next != NULL) {
      if(finlist->next == thread) {  finlist->next = NULL;  }
      else                        {  finlist = finlist->next;  }
    }
  }
}

/* finds, removes and delievers the thread that has waited the longest. */
struct green_t* last(green_t** list) {
  green_t* finlist = *list;
  //if(finlist == NULL) { printf("Deadlock, no thread in ready list.\n"); }
  /*else {*/
  if(finlist != NULL) {
    while(finlist->next != NULL) {  finlist = finlist->next;  }
    detatch(finlist, list);
  }
  return finlist;
}

/* when the timer expires the handler (this function) will be called and it is
time to schedile the next thread. */
void timer_handler(int sig) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t *susp = running;
  insert(susp, &flist);
  struct green_t *next = last(&flist);
  running = next;
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  swapcontext(susp->context, next->context);
}

/* sets initial values. Before section 4 it only contained getcontext() */
void init() {
  sigemptyset(&block);
  sigaddset(&block, SIGVTALRM);

  struct sigaction act = {0};
  struct timeval interval;
  struct itimerval period;

  act.sa_handler = timer_handler;
  assert(sigaction(SIGVTALRM, &act, NULL) == 0);

  interval.tv_sec = 0;
  interval.tv_usec = PERIOD;
  period.it_interval = interval;
  period.it_value = interval;
  setitimer(ITIMER_VIRTUAL, &period, NULL);

  getcontext(&main_cntx);
}

/* Starts execution of the real function and, after, terminate the thread. */
void green_thread() {
  sigprocmask(SIG_BLOCK, &block, NULL);

  green_t *this = running;
  void *result = (*this->fun)(this->arg);

  sigprocmask(SIG_UNBLOCK, &block, NULL);

  if(this->join != NULL){
    insert(this->join, &flist);
  }

  this->retval = &result;

  this->zombie = TRUE;
  struct green_t *next = last(&flist);
  running = next;
  setcontext(next->context);
}

/* Takes the running thread and places it last in the ready queue */
int green_yield() {
  sigprocmask(SIG_BLOCK, &block, NULL);
  green_t * susp = running;
  insert(susp, &flist);

  struct green_t *next = last(&flist);
  running = next;

  swapcontext(susp->context, next->context);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* Waits for a thread to terminate. If it is done already, nothing happens, otherwise:
The waiting thread is put as "join" to the thread it is waiting for.
Another thread is selected for execution. */
int green_join(green_t *thread, void **res) {
  sigprocmask(SIG_BLOCK, &block, NULL);
  if(!thread->zombie) {
    green_t *susp = running;
    thread->join = susp;
    struct green_t *next = last(&flist);
    running = next;
    swapcontext(susp->context, next->context);
    return 0;
  }
  thread->retval = res;

  free(thread->context->uc_stack.ss_sp);

  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* creates a new thread */
int green_create(green_t *new, void *(*fun)(void*), void *arg) {
  ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
  getcontext(cntx);
  void *stack = malloc(STACK_SIZE);

  cntx->uc_stack.ss_sp = stack;
  cntx->uc_stack.ss_size = STACK_SIZE;
  makecontext(cntx, green_thread, 0);

  new->context = cntx;
  new->fun = fun;
  new->arg = arg;
  new->next = NULL;
  new->join = NULL;
  new->retval = NULL;
  new->zombie = FALSE;

  sigprocmask(SIG_BLOCK, &block, NULL);
  insert(new, &flist);
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

void green_cond_init(struct green_cond_t *cond) {
  cond->next = NULL;
}

/* suspends the current thread */
int green_cond_wait(struct green_cond_t* cond, green_mutex_t* mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  insert(running, &(cond->next));
  green_t *temp = running;

  if(mutex != NULL) {
    //release the lock if we have a mutex
    mutex->taken = FALSE;
    //schedule suspended threads
    green_mutex_unlock(mutex);
  }

  running = last(&flist);
  swapcontext(temp->context, running->context);

  if(mutex != NULL) {
    //try to take the lock
    if(mutex->taken) {
      //suspend
      green_mutex_lock(mutex);
    } else {
      //take the lock
      mutex->taken = TRUE;
    }
  }

  sigprocmask(SIG_UNBLOCK, &block, NULL);

  return 0;
}

/* move the first suspended thread to the ready queue */
void green_cond_signal(struct green_cond_t *cond) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  green_t *next = last(&(cond->next));
  insert(next, &flist);

  sigprocmask(SIG_UNBLOCK, &block, NULL);
}

/* initialize a green condition variable */
int green_mutex_init(green_mutex_t *mutex) {
  mutex->taken = FALSE;
  mutex->next = NULL;
  return 0;
}

/* function that tries to take the lock. If lock is taken: "pass the baton".
A thread that holds the lock will give the lock to the next thread in line.
If we wake up after having suspended on the mutex we know that the lock is ours. */
int green_mutex_lock(green_mutex_t *mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  green_t *susp = running;
  if(mutex->taken) {
    insert(susp, &(mutex->next));
    green_t *next = last(&flist);
    running = next;
    swapcontext(susp->context, running->context);
  } else {
    //take the lock
    mutex->taken = TRUE;
  }
  sigprocmask(SIG_UNBLOCK, &block, NULL);
  return 0;
}

/* If there is a thread waiting on the lock we do not release the lock but pass
it over to the suspended thread. */
int green_mutex_unlock(green_mutex_t *mutex) {
  sigprocmask(SIG_BLOCK, &block, NULL);

  if(mutex->next != NULL) {
    green_t* temp = mutex->next;
    if(temp != NULL)  {
      insert(temp, &flist);
      while(temp->next != NULL){
        temp = temp->next;
        insert(temp, &flist);
      }
    }
    mutex->next = NULL;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 1;
  } else {
    //release lock
    mutex->taken = FALSE;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
  }
}
