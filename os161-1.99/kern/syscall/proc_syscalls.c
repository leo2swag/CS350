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
#include "opt-A2.h"
#include <mips/trapframe.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <test.h>
#include <sfs.h>
#include <synch.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

#ifdef OPT_A2

void forked_process(void *tf, unsigned long data) {
  struct trapframe tflocal = *((struct trapframe *) tf);
  tflocal.tf_a3 = 0;
  tflocal.tf_v0 = 0;
  tflocal.tf_epc += 4;
  mips_usermode(&tflocal);
  kfree(tf);
  (void) data;
}

int 
sys_fork(struct trapframe *parent_tf, pid_t *retval) {
  //create process
  struct proc *child = proc_create_runprogram(curproc->p_name);
  if (child == NULL) {
    return ENOMEM; //out of memory code 
  }

  //create address space
  struct addrspace *child_addre;
  int copy_det = as_copy(curproc->p_addrspace, &child_addre);
  if (copy_det) {
    proc_destroy(child);
    return copy_det;
  }

  lock_acquire(child->proc_lock);
  child->p_addrspace = child_addre;
  //assign pid
  child->parent_pid = curproc->pid;
  lock_release(child->proc_lock);

  //assign to children
  lock_acquire(childprocs_lock);
  childprocs[child->pid] = child;
  lock_release(childprocs_lock);

  //if curporc, assign to parent
  lock_acquire(parentprocs_lock);
  if (curproc->parent_pid == -1) {
	  parentprocs[curproc->pid] = curproc;
  }
  lock_release(parentprocs_lock);
  

  //create thread
  struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  memcpy(child_tf, parent_tf, sizeof(struct trapframe));
  int thread_det = thread_fork(child->p_name, child, forked_process, child_tf, 0);
  if (thread_det) {
    kfree(child_addre);
    proc_destroy(child);
    return thread_det;
  }

  *retval = child->pid;
  return 0;
}

#endif

void sys__exit(int exitcode) {

	struct addrspace *as;
	struct proc *p = curproc;
	/* for now, just include this to keep the compiler from complaining about
	   an unused variable */
#ifdef OPT_A2


	   /*
	   lock_acquire(childprocs_lock);
	   struct proc *pp = childprocs[p->parent_pid];
	   if (pp != NULL) {
		   pp->childexit[p->pid] = _MKWAIT_EXIT(exitcode);
	   }
	   childprocs[p->pid] = NULL;
	   lock_release(childprocs_lock);
	   */

	if (p->parent_pid != -1) { //curproc is not origin parent
		struct proc *parent = allprocs[p->parent_pid];
		lock_acquire(parent->proc_lock);
		if (parent != NULL) {
			p->ifalive = false;
			p->exitcode = _MKWAIT_EXIT(exitcode);
		}
		lock_release(parent->proc_lock);

		lock_acquire(childprocs_lock);
		childprocs[p->pid] = NULL;
		lock_release(childprocs_lock);


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
  #ifdef OPT_A2
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
  /* for now, just pretend the exitstatus is 0 */
#ifdef OPT_A2
  lock_acquire(childprocs_lock);
  struct proc *child = childprocs[pid];
  //if (child == NULL) {
//	  return ESRCH;
  //}
  lock_release(childprocs_lock);

  lock_acquire(child->proc_lock);
  if (child != NULL) {
	  cv_wait(child->proc_cv, child->proc_lock);
  }
  exitstatus = child->exitcode;
  lock_release(child->proc_lock);


  //lock_acquire(curproc->proc_lock);
  /*
  if (child->parent_pid != curproc->pid) {
	  return EPERM;
  }
  if (child->ifalive) {
	  cv_wait(child->proc_cv, child->proc_lock);
  }*/
  //exitstatus = child->exitcode;
  //exitstatus = curproc->childexit[pid];
  //lock_release(curproc->proc_lock);


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
