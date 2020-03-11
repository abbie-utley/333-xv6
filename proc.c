#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif //CS333P2

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};

#ifdef CS333_P3
// record with head and tail pointer for constant-time access to the beginning
// and end of a linked list of struct procs.  use with stateListAdd() and
// stateListRemove().
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif

#ifdef CS333_P3
#define statecount NELEM(states)
#endif //CS333_P3


static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3
  struct ptrs list[statecount];
#endif //CS333_P3
#ifdef CS333_P4
  struct ptrs ready[MAXPRIO + 1];
  uint PromoteAtTime;
#endif //CS333_P4
} ptable;

// list management function prototypes
#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate, const char *, int);
#endif //CS333_P3

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

  void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
  struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; i++) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
  static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

//overwrite P3
#ifdef CS333_P4 
  acquire(&ptable.lock);

  p = ptable.list[UNUSED].head;
  if(p != NULL)
  {
    if(stateListRemove(&ptable.list[UNUSED], p) == -1)
    {
      panic("process was not able to be removed from unused list in allocproc");
    }
    assertState(p, UNUSED, __FUNCTION__, __LINE__);
    p->state = EMBRYO;
    stateListAdd(&ptable.list[EMBRYO], p);
    p->pid = nextpid++;
    //set the priority and budget
    p->priority = MAXPRIO;
    p->budget = DEFAULT_BUDGET;
    release(&ptable.lock);
  }
  else
  {
    release(&ptable.lock);
    return 0;
  }

#else // do this if not in P3
  acquire(&ptable.lock);

  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }

  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

#endif // end of ifdef / else statement

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    // This happens if kalloc() can't use any memory
#ifdef CS333_P3
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
    {
      panic("Cannot remove from embryo list (happening in if p->stack = kalloc in allocproc()");
    }
    assertState(p, EMBRYO, __FUNCTION__, __LINE__);
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
    release(&ptable.lock);

#else // do this if not in CS333_P3
    p->state = UNUSED;
#endif // finish ifdef / else statement
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

#ifdef CS333_P1
  p->start_ticks = ticks;
#endif //CS333_P1

#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif //CS333_P2

  //priority and budget allocation
#ifdef CS333_P4
#endif //CS333_P4
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
  void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

#ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif //CS333_P3

  //inititalize PromoteAtTime
#ifdef CS333_P4
  acquire(&ptable.lock);
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  release(&ptable.lock);
#endif //CS333_P3

  p = allocproc();
#ifdef CS333_P2
  p->uid = DEF_UID;
  p->gid = DEF_GID;
#endif //CS333_P2


  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  // This is where userinit takes the only process in userinit, and changes it to 
  // runnable. if not in CS333_P3 then just change the state

#ifdef CS333_P4
  if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
  {
    panic("process not removed from embryo state in userinit");
  }
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
  p->state = RUNNABLE;
  stateListAdd(&ptable.ready[p->priority], p);
  p->budget = DEFAULT_BUDGET;

#elif defined(CS333_P3)
  if(stateListRemove(&ptable.list[EMBRYO], p) == -1)
  {
    panic("process not removed from embryo state in userinit");
  }
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
  p->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], p);

#else // after CS333_P3
  p->state = RUNNABLE;
#endif //end if ifdef / else statement

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
  int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
  int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;

    //do this if p3 is on
#ifdef CS333_P3
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[EMBRYO], np) == -1)
    {
      panic("Error: unable to take off of embryo list. Happening in fork at the copy process");
    }
    assertState(np, EMBRYO, __FUNCTION__, __LINE__);
    np->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], np);
    release(&ptable.lock);

#else // do this if not p3
    np->state = UNUSED;

#endif //end of ifdef / else statement
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;


  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  // do this if in p4
#ifdef CS333_P4
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[EMBRYO], np) == -1)
  {
    panic("Error at fork copying the parent process (line 371)");
  }
  assertState( np, EMBRYO, __FUNCTION__, __LINE__);
  np->state = RUNNABLE;
  stateListAdd(&ptable.ready[np->priority], np);
  release(&ptable.lock);

#elif defined(CS333_P3) // for P3
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[EMBRYO], np) == -1)
  {
    panic("Error at fork copying the parent process (line 371)");
  }
  assertState( np, EMBRYO, __FUNCTION__, __LINE__);
  np->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], np);
  release(&ptable.lock);

#else //do this if not in p3 or P4

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
#endif // end of ifdef / else statement

  //have fork copy the uid and gid of the parents 
#ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
#endif //CS333_P2

  return pid;

}


#ifdef CS333_P4


// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
  void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  //for the ready list
  for(int i = 0; i <= MAXPRIO; ++i)
  {
    p = ptable.ready[i].head;
    while(p!= NULL)
    {
      if(p->parent == curproc)
      {
        p->parent = initproc;
      }
      if(p->state == ZOMBIE)
      {
        wakeup1(initproc);
      }
      p = p->next;
    }
  }

  for(int i = EMBRYO; i <= ZOMBIE; i++)
  {
    p = ptable.list[i].head;
    while(p!= NULL)
    {
      if(p->parent == curproc)
      {
        p->parent = initproc;
      }
      if(p->state == ZOMBIE)
      {
        wakeup1(initproc);
      }
      p = p->next;
    }
  }


  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("cannot remove from RUNNING in exit()");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}

//for CS333 project's exit function

#elif defined(CS333_P3)


// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
  void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  for(int i = EMBRYO; i <= ZOMBIE; i++)
  {
    p = ptable.list[i].head;
    while(p!= NULL)
    {
      if(p->parent == curproc)
      {
        p->parent = initproc;
      }
      if(p->state == ZOMBIE)
      {
        wakeup1(initproc);
      }
      p = p->next;
    }
  }


  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("cannot remove from RUNNING in exit()");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}

// this is for not CS333 projects
#else //not CS333_P3

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
  void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}

#endif //Ending the ifdef / else statement


// this is for project 4 implementation
#ifdef CS333_P4

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
  int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = 0; i <= MAXPRIO; ++i)
    {
      p = ptable.ready[i].head;
      while(p != NULL)
      {
        if(p->parent != curproc)
        {
          p = p->next;
          continue;
        }
        havekids = 1;
        p = p->next;
      }
    }

    for(int i = EMBRYO; i <= ZOMBIE; i++)
    {
      p = ptable.list[i].head;
      while(p != NULL)
      {
        if(p->parent != curproc)
        {
          p = p->next;
          continue;
        }
        havekids = 1;
        if(p->state == ZOMBIE)
        {
          if(stateListRemove(&ptable.list[ZOMBIE], p) == -1)
          {
            panic("Unable to remove from ZOMBIE in wait()");
          }
          assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          p->state = UNUSED;
          stateListAdd(&ptable.list[UNUSED], p);
          release(&ptable.lock);
          return pid;
        }
        p = p->next;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// this is for project 3 implementation
#elif defined(CS333_P3)

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
  int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(int i = EMBRYO; i <= ZOMBIE; i++)
    {
      p = ptable.list[i].head;
      while(p != NULL)
      {
        if(p->parent != curproc)
        {
          p = p->next;
          continue;
        }
        havekids = 1;
        if(p->state == ZOMBIE)
        {
          if(stateListRemove(&ptable.list[ZOMBIE], p) == -1)
          {
            panic("Unable to remove from ZOMBIE in wait()");
          }
          assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          p->state = UNUSED;
          stateListAdd(&ptable.list[UNUSED], p);
          release(&ptable.lock);
          return pid;
        }
        p = p->next;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//for projects not CS333_P3

#else

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
  int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

#endif //ending the ifdef / else statement


//This is for project 4
#ifdef CS333_P4

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
  void
scheduler(void)
{
  int i;
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);

    //check for promotion
    if(ticks >= ptable.PromoteAtTime && MAXPRIO != 0)
    {
      //call function for promotion which takes no arguments
      promotion();
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }


#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.

    //add code for scheduler here
    for(i = MAXPRIO; i >= 0; --i)
    {
      p = ptable.ready[i].head;
      if(p != NULL)
        break;
    }
    if(p != NULL)
    {
      if(stateListRemove(&ptable.ready[i], p) == -1)
      {
        panic("Unable to take off of Ready list in scheduler");
      }
      if(p->priority != i)
        panic("not at the top of the ready list");

      p->state = RUNNING;
      stateListAdd(&ptable.list[RUNNING], p);



      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;
#endif //PDX_XV6
      c->proc = p;
      switchuvm(p);


      //need time in for CPU
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif //CS333_P2

      swtch(&(c->scheduler), p->context);
      switchkvm();


      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

//This is for project 3
#elif defined(CS333_P3)

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
  void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.

    //add code for scheduler here
    acquire(&ptable.lock);
    p = ptable.list[RUNNABLE].head;
    if(p !=NULL)
    {

      if(stateListRemove(&ptable.list[RUNNABLE], p) == -1)
      {
        panic("Unable to take off of RUNNING list in scheduler");
      }
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);

      p->state = RUNNING;
      stateListAdd(&ptable.list[RUNNING], p);



      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;
#endif //PDX_XV6
      c->proc = p;
      switchuvm(p);


      //need time in for CPU
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif //CS333_P2

      swtch(&(c->scheduler), p->context);
      switchkvm();


      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

//for non project 3 stuff
#else

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
  void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      //need time in for CPU
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif //CS333_P2

      swtch(&(c->scheduler), p->context);
      switchkvm();


      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#endif //Ending the ifdef / else statement

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
  void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;

  //measure the cpu time
#ifdef CS333_P2
  p->cpu_ticks_total += (ticks - p->cpu_ticks_in);
#endif //CS333_P2

  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

#ifdef CS333_P4
// Give up the CPU for one scheduling round.
  void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock

  curproc->budget = curproc->budget - (ticks - curproc->cpu_ticks_in);
  if(curproc->budget <= 0)
  {
    curproc->priority -= 1;
    if(curproc->priority < 0)
      curproc->priority = 0;
    if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
      panic("unable to remove from RUNNING in yield");
    assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
    curproc->state = RUNNABLE;
    curproc->budget = DEFAULT_BUDGET;
    stateListAdd(&ptable.ready[curproc->priority], curproc);
  }
  else
  {
    if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
      panic("unable to remove from RUNNING in yield");
    assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
    curproc->state = RUNNABLE;
    stateListAdd(&ptable.ready[curproc->priority], curproc);
  }
  sched();
  release(&ptable.lock);
}

//this is for project 3
#elif defined(CS333_P3)

// Give up the CPU for one scheduling round.
  void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock

  if(stateListRemove(&ptable.list[RUNNING], curproc) == -1)
  {
    panic("unable to remove from RUNNING in yield");
  }
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], curproc);
  sched();
  release(&ptable.lock);
}

//for non-project 3 or  stuff
#else

// Give up the CPU for one scheduling round.
  void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

#endif //ending the ifdef / else statement

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
  void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

//for project 4
#ifdef CS333_P4

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
  void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  //demote if necessary
  p->budget = p->budget - (ticks - p->cpu_ticks_in);
  if(p->budget <= 0)
  {
    p->priority -= 1;
    if(p->priority < 0)
      p->priority = 0;
    p->budget = DEFAULT_BUDGET;
  }
  if(stateListRemove(&ptable.list[RUNNING], p) == -1)
  {
    panic("Cannot remove from running list (happening in sleep)");
  }
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
  p->chan = chan;
  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//for project 3: 
#elif defined(CS333_P3)

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
  void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  if(stateListRemove(&ptable.list[RUNNING], p) == -1)
  {
    panic("Cannot remove from running list (happening in sleep)");
  }
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
  p->chan = chan;
  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//for non project 3 or 4 projects 
#else

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
  void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#endif //ending the ifdef / else statement

//for project 4

#ifdef CS333_P4

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
  static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *temp;

  p = ptable.list[SLEEPING].head;
  while(p != NULL)
  {
    if(p->chan == chan)
    {
      temp = p->next;
      if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
      {
        panic("Cannot remove from sleeping state in wakeup1");
      }
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
      p = temp;
    }
    else
    {
      p = p->next;
    }
  }
}

//for project 3:
#elif defined(CS333_P3)

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
  static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *temp;

  p = ptable.list[SLEEPING].head;
  while(p != NULL)
  {
    if(p->chan == chan)
    {
      temp = p->next;
      if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
      {
        panic("Cannot remove from sleeping state in wakeup1");
      }
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;

      stateListAdd(&ptable.list[RUNNABLE], p);
      p = temp;
    }
    else
    {
      p = p->next;
    }
  }
}


//for non-project 3:
#else

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
  static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

#endif //ending the ifdef / else statement

// Wake up all processes sleeping on chan.
  void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}


//for project 4:
#ifdef CS333_P4

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
  int
kill(int pid)
{
  struct proc *p;
  struct proc *temp;

  acquire(&ptable.lock);

  //check the ready list first
  for(int i = 0; i <= MAXPRIO; ++i)
  {
    p = ptable.ready[i].head;
    while(p)
    {
      if(p->pid == pid)
      {
        p->killed = 1; //kill process
        //you don't need to wake it up from sleeping because the sleeping list 
        //should not be in the ready list
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }

  for(int i = EMBRYO; i < ZOMBIE; i++)
  {
    p = ptable.list[i].head;
    while(p)
    {
      if(p->pid == pid)
      {
        p->killed = 1; //kill process
        //wake up from sleep if necessary
        if(p->state == SLEEPING)
        {
          temp = p->next;
          if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
          {
            panic("Cannot remove from sleeping list in kill()");
          }
          assertState(p, SLEEPING, __FUNCTION__, __LINE__);
          p->state = RUNNABLE;

          stateListAdd(&ptable.ready[p->priority], p);

          p = temp;
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

//for project 3:
#elif defined(CS333_P3)

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
  int
kill(int pid)
{
  struct proc *p;
  struct proc *temp;

  acquire(&ptable.lock);

  for(int i = EMBRYO; i < ZOMBIE; i++)
  {
    p = ptable.list[i].head;
    while(p != NULL) 
    {
      if(p->pid == pid)
      {
        p->killed = 1; //kill process
        //wake up from sleep if necessary
        if(p->state == SLEEPING)
        {
          temp = p->next;
          if(stateListRemove(&ptable.list[SLEEPING], p) == -1)
          {
            panic("Cannot remove from sleeping list in kill()");
          }
          assertState(p, SLEEPING, __FUNCTION__, __LINE__);
          p->state = RUNNABLE;

          stateListAdd(&ptable.list[RUNNABLE], p);
          p = temp;
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

//for non-project 3:
#else

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
  int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

#endif //ending the ifdef / else statement

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#if defined(CS333_P4)
  void
procdumpP4(struct proc *p, char *state_string)
{

  int CPUSEC, CPUTEN, CPUHUND, CPUMILLI;
  int T_SEC, T_TEN, T_HUND, T_MILLI;
  uint ppid;

  if(p->parent == NULL)
  {
    ppid = p->pid;
  }
  else
  {
    ppid = p->parent->pid;
  }

  CPUSEC = p->cpu_ticks_total;
  CPUTEN = CPUSEC % 1000;
  CPUSEC /= 1000;
  CPUHUND = CPUTEN % 100;
  CPUTEN /= 100;
  CPUMILLI = CPUHUND % 10;
  CPUHUND /= 10;

  T_SEC = ticks - p->start_ticks;
  T_TEN = T_SEC % 1000;
  T_SEC /= 1000;
  T_HUND = T_TEN % 100;
  T_TEN /= 100;
  T_MILLI = T_HUND % 10;
  T_HUND /= 10;

  if(strlen(p->name) > 8)
  {
    cprintf("\n%d\t%s\t%d\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, p->priority, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC, CPUTEN, CPUHUND, CPUMILLI, state_string, p->sz);
    return;
  }

  cprintf("\n%d\t%s\t\t%d\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, p->priority, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC, CPUTEN, CPUHUND, CPUMILLI, state_string, p->sz);
  return;

}
#elif defined(CS333_P3)
  void
procdumpP3(struct proc *p, char *state_string)
{
  int CPUSEC, CPUTEN, CPUHUND, CPUMILLI;
  int T_SEC, T_TEN, T_HUND, T_MILLI;
  uint ppid;

  if(p->parent == NULL)
  {
    ppid = p->pid;
  }
  else
  {
    ppid = p->parent->pid;
  }

  CPUSEC = p->cpu_ticks_total;
  CPUTEN = CPUSEC % 1000;
  CPUSEC /= 1000;
  CPUHUND = CPUTEN % 100;
  CPUTEN /= 100;
  CPUMILLI = CPUHUND % 10;
  CPUHUND /= 10;

  T_SEC = ticks - p->start_ticks;
  T_TEN = T_SEC % 1000;
  T_SEC /= 1000;
  T_HUND = T_TEN % 100;
  T_TEN /= 100;
  T_MILLI = T_HUND % 10;
  T_HUND /= 10;

  if(strlen(p->name) > 8)
  {
    cprintf("\n%d\t%s\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC, CPUTEN, CPUHUND, CPUMILLI, state_string, p->sz);
    return;
  }

  cprintf("\n%d\t%s\t\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC, CPUTEN, CPUHUND, CPUMILLI, state_string, p->sz);
  return;
}
#elif defined(CS333_P2)
void procdumpP2(struct proc *p, char *state_string)
{
  int CPUSEC, CPUTEN, CPUHUND, CPUMILLI;
  int T_SEC, T_TEN, T_HUND, T_MILLI;
  uint ppid;

  if(p->parent == NULL)
  {
    ppid = p->pid;
  }
  else
  {
    ppid = p->parent->pid;
  }

  CPUSEC = p->cpu_ticks_total;
  CPUTEN = CPUSEC % 1000;
  CPUSEC /= 1000;
  CPUHUND = CPUTEN % 100;
  CPUTEN /= 100;
  CPUMILLI = CPUHUND % 10;
  CPUHUND /= 10;

  T_SEC = ticks - p->start_ticks;
  T_TEN = T_SEC % 1000;
  T_SEC /= 1000;
  T_HUND = T_TEN % 100;
  T_TEN /= 100;
  T_MILLI = T_HUND % 10;
  T_HUND /= 10;

  cprintf("\n%d\t%s\t\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC, CPUTEN, CPUHUND, CPUMILLI, state_string, p->sz);


  return;
}
#elif defined(CS333_P1)
  void
procdumpP1(struct proc *p, char *state_string)
{
  int milli = ticks - p->start_ticks;
  int sec = milli/1000;
  milli %=1000;
  cprintf("\n%d\t%s\t\t%d.%d\t%s\t%d\t", p->pid, p->name, sec, milli, state_string, p->sz);
}
#endif

  void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tPrio\tElapsed\t\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P3)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\t\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\t\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName         Elapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);  // not conditionally compiled as must work in all project states

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    // see TODOs above this function
#if defined(CS333_P4)
    procdumpP4(p, state);
#elif defined(CS333_P3)
    procdumpP3(p, state);
#elif defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
#ifdef CS333_P1
  cprintf("$ ");  // simulate shell prompt
#endif // CS333_P1
}

#if defined(CS333_P3)
// list management helper functions
  static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}
#endif

#if defined(CS333_P3)
  static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}
#endif

#if defined(CS333_P3)
  static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#if defined(CS333_P4)
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}
#endif

#if defined(CS333_P3)
  static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}
#endif

#if defined(CS333_P3)
// example usage:
// assertState(p, UNUSED, __FUNCTION__, __LINE__);
// This code uses gcc preprocessor directives. For details, see
// https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
  static void
assertState(struct proc *p, enum procstate state, const char * func, int line)
{
  if (p->state == state)
    return;
  cprintf("Error: proc state is %s and should be %s.\nCalled from %s line %d\n",
      states[p->state], states[state], func, line);
  panic("Error: Process state incorrect in assertState()");
}
#endif

#ifdef CS333_P2
  int
getprocs(uint max, struct uproc * table)
{
  //variable declaration
  struct proc *p;
  char * state;
  int i = 0;

  //aquire the lock
  acquire(&ptable.lock);

  p = ptable.proc;

  do
  {

    if(p->state != UNUSED || p->state != EMBRYO)
    {
      table[i].pid = p->pid;
      table[i].uid = p->uid;
      table[i].gid = p->gid;
      if(p->parent == NULL)
      {
        table[i].ppid = p->pid;
      }
      else
      {
        table[i].ppid = p->parent->pid;
      }
      table[i].elapsed_ticks = ticks - p->start_ticks;
      table[i].CPU_total_ticks = p->cpu_ticks_total;
      table[i].size = p->sz;

#ifdef CS333_P4
      table[i].priority = p->priority;
#endif //CS333_P4

      state = states[p->state];
      safestrcpy(table[i].state, state, STRMAX);
      safestrcpy(table[i].name, p->name, STRMAX);

      i++;
    }
    p++;

  }while(p < &ptable.proc[NPROC] && i < max);

  release(&ptable.lock);

  return i;
}
#endif //CS333_P2

// this works control F and prints the amount of unused processes
#ifdef CS333_P3
  void
controlF(void)
{
  int count = 0;

  acquire(&ptable.lock);
  if(&ptable.list[UNUSED].head == NULL)
  {
    cprintf("Free List Size: %d processes\n", count);
  }
  struct proc * curr = ptable.list[UNUSED].head;
  while(curr != NULL)
  {
    ++count;
    curr = curr->next;
  }
  cprintf("Free List Size: %d processes\n", count);
  release(&ptable.lock);
  //simulate shell prompt:
  cprintf("$ ");
  return;
}

//test function to measure the stats for a list
// I tried to put it in its own test file
// however too many problems arise and it made me tired
// so it's time to clutter proc.c again
// this was given to us by Mark in https://web.cecs.pdx.edu/~markem/CS333/handouts/printListStats.c
  void
printListStats(void)
{
  int i, count, total = 0;
  struct proc *p;

  acquire(&ptable.lock);
  for (i=UNUSED; i<=ZOMBIE; i++) {
    count = 0;
    p = ptable.list[i].head;
    while (p != NULL) {
      count++;
      p = p->next;
    }
    cprintf("\n%s list has ", states[i]);
    if (count < 10) cprintf(" ");  // line up columns
    cprintf("%d processes", count);
    total += count;
  }
  release(&ptable.lock);
  cprintf("\nTotal on lists is: %d. NPROC = %d. %s",
      total, NPROC, (total == NPROC) ? "Congratulations!" : "Bummer");
  cprintf("\n$ ");  // simulate shell prompt
  return;
}

//function for control R - prints runnable list
void controlR(void)
{
  acquire(&ptable.lock);
  struct proc * curr = ptable.list[RUNNABLE].head;
  if(curr == NULL)
  {
    cprintf("There are no Runnable processes\n$ ");
    release(&ptable.lock);
    return;
  }
  cprintf("Ready List Processes:\n");
  while(curr != NULL)
  {
    if(curr->next == NULL)
    {
      cprintf("%d\n$ ", curr->pid); //simulate shell prompt after last process
    }
    else {
      cprintf("%d -> ", curr->pid);
    }
    curr = curr->next;
  }
  release(&ptable.lock);
  return;
}

//function for controlS to print the sleeping list
void controlS(void)
{
  acquire(&ptable.lock);
  struct proc * curr = ptable.list[SLEEPING].head;
  if(curr == NULL)
  {
    cprintf("There are no sleeping processes\n$ ");
    release(&ptable.lock);
    return;
  }
  cprintf("Sleep list processes:\n");
  while(curr != NULL)
  {
    if(curr->next == NULL)
    {
      cprintf("%d\n$ ", curr->pid); //simulate shell prompt after last process
    }
    else {
      cprintf("%d -> ", curr->pid);
    }
    curr = curr->next;
  }
  release(&ptable.lock);
  return;
}

//function for controlZ to print the zombie list
void controlZ(void)
{
  acquire(&ptable.lock);
  struct proc * curr = ptable.list[ZOMBIE].head;
  if(curr == NULL)
  {
    cprintf("There are no zombie processes\n$ "); //simulate the shell prompt
    release(&ptable.lock);
    return;
  }
  cprintf("Zombie list processes:\n");
  while(curr != NULL)
  {
    if(curr->next == NULL)
    {
      cprintf("(%d, %d)\n$ ", curr->pid, curr->parent->pid); //simulate shell prompt after last process
    }
    else {
      cprintf("(%d, %d) -> ", curr->pid, curr->parent->pid);
    }
    curr = curr->next;
  }
  release(&ptable.lock);
  return;
}

#endif //CS333_P3

#ifdef CS333_P4
//set the priority of a process in the proc table
int 
setpriority(int pid, int priority)
{
  struct proc * curr;

  acquire(&ptable.lock);
  for(int i = EMBRYO; i <= ZOMBIE; ++i)
  {
    curr = ptable.list[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        curr->priority = priority;
        curr->budget = DEFAULT_BUDGET;
        release(&ptable.lock);
        return 0;
      }
      curr = curr->next;
    }
  }
  //Look through the ready lists as well
  for(int i = 0; i <= MAXPRIO; ++i)
  {
    curr = ptable.ready[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        curr->priority = priority;
        curr->budget = DEFAULT_BUDGET;
        release(&ptable.lock);
        return 0;
      }
      curr = curr->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

//get the priorty of a process in the proc table
int 
getpriority(int pid)
{
  struct proc * curr;

  acquire(&ptable.lock);
  for(int i = EMBRYO; i <= ZOMBIE; ++i)
  {
    curr = ptable.list[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        release(&ptable.lock);
        return curr->priority;
      }
      curr = curr->next;
    }
  }
  //look through the ready list as well 
  for(int i = 0; i <= MAXPRIO; ++i)
  {
    curr = ptable.ready[i].head;
    while(curr)
    {
      if(curr->pid == pid)
      {
        release(&ptable.lock);
        return curr->priority;
      }
      curr = curr->next;
    }
  }
  release(&ptable.lock);
  return -1;

}

void 
promotion(void)
{

  //first check the sleeping list to easily adjust
  struct proc *p;
  struct proc *temp;
  p = ptable.list[SLEEPING].head;
  while(p)
  {
    p -> budget = DEFAULT_BUDGET;
    if(p->priority < MAXPRIO)
    {
      p->priority += 1;
    }
    p = p->next;
  }

  //next check the running list
  p = ptable.list[RUNNING].head;
  while(p)
  {
    p->budget = DEFAULT_BUDGET;
    if(p->priority != MAXPRIO)
    {
      p -> priority += 1;
    }
    p = p->next;
  }

  //now for the hard part .... the READY LISTS! REST IN PIECES
  for(int i = (MAXPRIO-1); i >= 0; --i)
  {
    p = ptable.ready[i].head;
    while(p)
    {
      temp = p->next;
      if(stateListRemove(&ptable.ready[i], p) == -1)
      {
        panic("unable to promote in scheduler");
      }
      if(i != p->priority)
        panic("not in the right list");

      p->priority += 1;
      p->budget = DEFAULT_BUDGET;
      stateListAdd(&ptable.ready[p->priority], p);
      p = temp;

    }
  }
}

void 
controlRP4(void)
{

  acquire(&ptable.lock);
  struct proc * curr = ptable.ready[MAXPRIO].head;
  //struct proc * run = ptable.list[RUNNING].head;
  int count = 0;
/*  if(run == NULL)
  {
    cprintf("There are currently no processes running.\n");
  }
  else
  {
    //comment out here for printing running process
    cprintf("Running processes:\n");
    while(run)
    {
      if(run->next == NULL)
      {
        cprintf("PID: %d, BUDGET LEFT: %d, PRIORITY: %d\n\n", run->pid, run->budget, run->priority);
      }
      else
      {
        cprintf("PID: %d, BUDGET LEFT: %d PRIORITY: %d -> ", run->pid, run->budget, run->priority);
      }
      run = run->next;
    }
  }*/
  cprintf("Ready List Processes:\n");
  for(int i = (MAXPRIO); i >= 0; --i)
  {
    curr = ptable.ready[i].head;
    if(i == MAXPRIO)
    {
      cprintf("MAXPRIO: ");
    }
    else
    {
      cprintf("MAXPRIO-%d: ", count);
    }
    while(curr)
    {
      if(curr->next == NULL)
      {
        cprintf("(PID: %d, BUDGET: %d)", curr->pid, curr->budget); 
      }
      else 
      {
        cprintf("(PID: %d, BUDGET: %d) -> ", curr->pid, curr->budget);
      }
      curr = curr->next;
    }
    cprintf("\n");
    ++count;
  }
  cprintf("\n\n$"); //simulate shell
  release(&ptable.lock);
  return;
}

#endif //CS333_P4
