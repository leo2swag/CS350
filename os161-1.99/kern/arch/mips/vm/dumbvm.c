/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include <opt-A3.h>

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12
#if OPT_A3
	struct Mapaddr {
		paddr_t proc_addr;
		int otherFrameNum;
	};
	struct Mapaddr * coremap;
	bool kern_call = true;
	int numofFrame = 0;
	int total_with_coremap_numofFrame = 0;
	paddr_t addr_new_lo;
	paddr_t addr_lo;
	paddr_t addr_hi;
#endif
/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{
	#if OPT_A3
	/*
	paddr_t addr_lo;
	paddr_t addr_hi;
	
	ram_getsize(&addr_lo, &addr_hi);
	//get the number of frames
	coremap = (struct Mapaddr *) PADDR_TO_KVADDR(addr_lo);
	//include coremap num of frames
	total_with_coremap_numofFrame = (addr_hi - addr_lo) / PAGE_SIZE;

	//set up the first proc address for core map[0]
	//should be right above the coremap addr
	
	addr_new_lo = ROUNDUP(addr_lo + (total_with_coremap_numofFrame * sizeof(struct Mapaddr)), PAGE_SIZE);
	coremap[0].proc_addr = addr_new_lo;
	coremap[0].otherFrameNum = 0;
	
	//update the numofFrame that does not include the coremap size
	numofFrame = (addr_hi - addr_new_lo) / PAGE_SIZE;

	//finish the coremap init
	for (int i = 1; i < numofFrame; i++) {
		addr_new_lo = addr_new_lo + PAGE_SIZE;
		coremap[i].proc_addr = addr_new_lo;
		coremap[i].otherFrameNum = 0;
	}
	kern_call = false;
*/
	ram_getsize(&addr_lo, &addr_hi);
	numofFrame = (addr_hi - addr_lo) / PAGE_SIZE;

	coremap = (struct Mapaddr *) PADDR_TO_KVADDR(addr_lo);

	addr_lo = addr_lo + numofFrame * sizeof(struct Mapaddr);
	addr_lo = ROUNDUP(addr_lo, PAGE_SIZE);

	numofFrame = (addr_hi - addr_lo) / PAGE_SIZE;

	for (int i = 0; i < numofFrame; ++i) {
		coremap[i].otherFrameNum = 0;
		
	}

	kern_call = false;

	#endif
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);
	#if OPT_A3
	/*
	int index = 0;
	int counter = 0;
	if (kern_call == false) { //not the kern_call
		for (int i = 0; i < numofFrame; i++) {
			if (coremap[i].otherFrameNum == 0) { //not in use
				counter = counter + 1;
				if (counter == (int)npages) {
					index = i;
					break;
				}
			} else { //current use
				counter = 0;
				if (counter == (int)npages) {
					index = i;
					break;
				}
			}
		}

		//return the addr
		int found = index - npages + 1;
		coremap[found].otherFrameNum = (int)npages;

		paddr_t test = addr_new_lo + found * PAGE_SIZE;
		int tt = 0;
		for (int i =0; i < numofFrame; i++) {
			if (coremap[i].proc_addr == test) {
				tt = i;
				break;
			}
		}
		kprintf("tt %d\n",tt);
		kprintf("ff %d\n",found);
		//addr = coremap[found].proc_addr;
		addr = test;
		KASSERT(test==addr);
		
		//update the rest core map
		for (int i = 0; i < (int) npages; i++) {
			coremap[found+i].otherFrameNum = (int)npages;
		}

	} else {
		addr = ram_stealmem(npages);
	}
	*/
	if (kern_call) {
		addr = ram_stealmem(npages);
	} else {
		int i = 0;
		int count = 0;
		for (; i < numofFrame; ++i) {
			if (coremap[i].otherFrameNum == 0) { 
				++count;
			} else { 
				count = 0;
			}
			if (count == (int)npages) { break; }
		}
		int start_index = i - npages + 1;
		for(i = 0; i < (int)npages; ++i) {
			coremap[start_index + i].otherFrameNum = (int)npages;
		}
		addr = addr_lo + start_index * PAGE_SIZE;
		kprintf("tt %d\n",i);
		kprintf("ff %d\n",start_index);
	}

	#else
	addr = ram_stealmem(npages);
	#endif
	spinlock_release(&stealmem_lock);
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	#if OPT_A3
		spinlock_acquire(&stealmem_lock);
		//find that addr first
		int init_state = 0;
		for (int i = 0; i < numofFrame; i++) {
			if (coremap[i].proc_addr == addr) {
				init_state = i;
				break;
			}
		}

		int test_index = (PADDR_TO_KVADDR(addr) - addr_new_lo) / PAGE_SIZE;
		KASSERT(test_index==init_state);
		kprintf("hi, we are here");
		//free that address and any contiguous frames
		int pagenum = coremap[init_state].otherFrameNum;
		for (int i = 0; i < pagenum; i++) {
			coremap[init_state + i].otherFrameNum = 0;
		}
		spinlock_release(&stealmem_lock);
	#else
	/* nothing - leak the memory. */

	(void)addr;
	#endif
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;
	#if OPT_A3
		bool if_read_only = false;
	#endif
	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
		case VM_FAULT_READONLY:
		#if OPT_A3
			return EFAULT;
		#else
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
		#endif
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
		#if OPT_A3
			if_read_only = true;
		#endif
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		#if OPT_A3
			if (if_read_only && as->hasloaded) {
				elo &= ~TLBLO_DIRTY;
			}
		#endif
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}
	#if OPT_A3
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		if (if_read_only && as->hasloaded) {
			elo &= ~TLBLO_DIRTY;
		}
		tlb_random(ehi, elo);
		splx(spl);
		return 0;

	#else
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
	#endif
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
	#if OPT_A3
	as->hasloaded = false;
	#endif
	return as;
}

void
as_destroy(struct addrspace *as)
{
#if OPT_A3
	free_kpages(PADDR_TO_KVADDR(as->as_pbase1));
	free_kpages(PADDR_TO_KVADDR(as->as_pbase2));
	free_kpages(PADDR_TO_KVADDR(as->as_stackpbase));
#else
	kfree(as);
#endif
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}
	
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	#if OPT_A3
		as->hasloaded = true;
		as_activate();
	#else
	(void)as;
	#endif
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
	
	*ret = new;
	return 0;
}
