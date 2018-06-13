#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <sfs.h>
#include <test.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

  #if OPT_A2
    if (p->parent_pid != -1) {
      lock_acquire(proc_table_lock);
      struct proc* parent = proc_table[p->parent_pid];
      if (parent != NULL) {
        parent->children_exit_code[p->pid] = _MKWAIT_EXIT(exitcode);    // Add exit code
      }
      proc_table[p->pid] = NULL;
      lock_release(proc_table_lock);

      lock_acquire(p->proc_lock);
      cv_signal(p->proc_cv, p->proc_lock);
      lock_release(p->proc_lock);
    }
  #else
    (void)exitcode;
  #endif

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
    *retval = curproc->pid;
  #else 
    *retval = 1;
  #endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  #if OPT_A2
    lock_acquire(proc_table_lock);
    struct proc* child_proc = proc_table[pid];
    if (child_proc != NULL) {     // 查看小孩是否死亡
      cv_wait(child_proc->proc_cv, proc_table_lock);
    }
    lock_release(proc_table_lock);

    lock_acquire(curproc->proc_lock);
    exitstatus = curproc->children_exit_code[pid];
    lock_release(curproc->proc_lock);
  #else
    exitstatus = 0;
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#ifdef OPT_A2

void entryptfn(void* a, unsigned long b) {
  (void)b;
  struct trapframe tf = *((struct trapframe *) a);
  kfree(a);
  tf.tf_v0 = 0;
  tf.tf_a3 = 0;
  tf.tf_epc += 4;
  mips_usermode(&tf);
}


int sys_fork(struct trapframe *parent_tf, pid_t* retval) {
  struct proc *p = curproc;
  struct proc *child_proc;
  struct addrspace *child_addrs;

  // Create children process
  child_proc = proc_create_runprogram(p->p_name);

  if (child_proc == NULL) { // ERROR
    return ENOMEM;
  }

  // Create and copy address space
  int as_copy_result = as_copy(curproc->p_addrspace, &child_addrs);

  if (as_copy_result != 0) {  // error handling
    proc_destroy(child_proc);
    return as_copy_result;
  }

  lock_acquire(child_proc->proc_lock);
  child_proc->p_addrspace = child_addrs;
  setParentChildRelationship(p, child_proc);
  lock_release(child_proc->proc_lock);


  // Make a copy of trapframe
  struct trapframe* cpy_tf = kmalloc(sizeof(struct trapframe));
  memcpy(cpy_tf, parent_tf, sizeof(struct trapframe));

  thread_fork(child_proc->p_name, child_proc, entryptfn, cpy_tf, 0);

  *retval = child_proc->pid;

  return 0;
}


#endif


