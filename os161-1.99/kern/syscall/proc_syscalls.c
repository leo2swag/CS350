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

int sys_execv(char *progname, char **args) {
    //extract space
    size_t newprogspace = (strlen(progname) + 1);
    char *newprog = kmalloc(sizeof(char *) * newprogspace);
    int result = copyinstr((const_userptr_t) progname, newprog, newprogspace, NULL);
    if (result) {
      return result;
    }

    //count number of args
    int counter = 0;
    int incre = 0;
    while (args[incre] != NULL) {
      counter++;
      incre++;
    }

    //copy ags into kernel
    char *kernelprogs[counter];
    for (int i = 0; i < counter; i++) {
      size_t argspace = strlen(args[i]) + 1;
      char *kprog = kmalloc(sizeof(char *) * argspace);
      kernelprogs[i] = kprog;
      result = copyinstr((const_userptr_t)args[i], kprog, argspace, NULL);
      if (result) {
        return result;
      }
    }

    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    //int result;

    /* Open the file. */
    char *progbackup = kstrdup(progbackup);
    result = vfs_open(progbackup, O_RDONLY, 0, &v);
    if (result) {
      return result;
    }
    kfree(progbackup);

    struct addrspace *oldas = curproc_getas();
    if (oldas != NULL) {
      curproc_setas(NULL);
    }
    /* We should be a new process. */
    KASSERT(curproc_getas == NULL);

    /* Create a new address space. */
    as = as_create();
    if (as ==NULL) {
      vfs_close(v);
      return ENOMEM;
    }

    /* Switch to it and activate it. */
    curproc_setas(as);
    as_activate();

    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if (result) {
      /* p_addrspace will go away when curproc is destroyed */
      vfs_close(v);
      return result;
    }

    /* Done with the file now. */
    vfs_close(v);


    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
      /* p_addrspace will go away when curproc is destroyed */
      return result;
    }

    int tablecounter = counter;
    char *argstable[tablecounter+1];
    for (int i = tablecounter; i > 0; i--) {
      if (i == tablecounter) {
        argstable[tablecounter] = NULL;
      } else {
        size_t totalsize = ROUNDUP(strlen(kernelprogs[i]) + 1, 8);
        stackptr = totalsize - stackptr;
        int result = copyoutstr(kernelprogs[i], (userptr_t)stackptr, totalsize, NULL);
        if (result) {
          return result;
        }
        argstable[i] = (char *)stackptr;
      }
    }

    size_t argsize = ROUNDUP(4 * (tablecounter + 1), 8);
    stackptr = argsize - stackptr;
    result = copyout(argstable, (userptr_t)stackptr, sizeof(char *) * (tablecounter + 1));
    if (result) {
      return result;
    }

 
    as_destroy(oldas);

    /* Warp to user mode. */
    enter_new_process(tablecounter, (userptr_t)stackptr,
          stackptr, entrypoint);
    
    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
  }


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

    lock_acquire(allprocs_lock);
    child->p_addrspace = child_addre;
    //assign pid
    proc_info_table[child->pid].parent_pid = curproc->pid;
    lock_release(allprocs_lock);

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

  lock_acquire(allprocs_lock);
  proc_info_table[p->pid].is_alive = false;
  proc_info_table[p->pid].exitcode = _MKWAIT_EXIT(exitcode);
  cv_signal(p->proc_cv, allprocs_lock); //weak it self
  lock_release(allprocs_lock);
  
/*
  if (p->hasparent) {
    struct proc *parent = allprocs[p->parent_pid];
    lock_acquire(parent->proc_lock);
    cv_signal(parent->proc_cv, parent->proc_lock);
    lock_release(parent->proc_lock);
  }
*/

    /*
  if (p->haschild) {
    struct array *childs = p->childarry;
    for(unsigned int i = 0; i < childs->num; i++) {
      struct proc *childproc = array_get(childs, i);
      if (childproc->ifalive) {
        cv_wait(childproc->proc_cv, p->proc_lock);
      }
    }
  }

	*/

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
  lock_acquire(allprocs_lock);
  struct proc *child = proc_info_table[pid].proc;
  if (child == NULL) {
    lock_release(allprocs_lock);
	  return ESRCH;
  }
  if (proc_info_table[pid].parent_pid!= curproc->pid) {
    lock_release(allprocs_lock);
    return EPERM;
  }

  if (proc_info_table[pid].is_alive) {
	  cv_wait(child->proc_cv, allprocs_lock); //let parent wait until child dead
  }

  exitstatus = proc_info_table[pid].exitcode;
  //kprintf("code%d", exitstatus);
  lock_release(allprocs_lock);

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
