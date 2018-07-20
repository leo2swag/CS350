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
		int alloc_num;
	};
	struct Mapaddr * coremap;
	bool kern_call = true;
	int numofFrame = 0;
	int total_with_coremap_numofFrame = 0;
	paddr_t addr_new_start;

#endif
/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void
vm_bootstrap(void)
{
	#if OPT_A3
	
	paddr_t addr_start;
	paddr_t addr_end;
	
	ram_getsize(&addr_start, &addr_end);
	//get the number of frames
	coremap = (struct Mapaddr *) PADDR_TO_KVADDR(addr_start);
	//include coremap num of frames
	total_with_coremap_numofFrame = (addr_end - addr_start) / PAGE_SIZE;

	//set up the first proc address for core map[0]
	//should be right above the coremap addr
	addr_new_start = ROUNDUP(addr_start + (total_with_coremap_numofFrame * sizeof(struct Mapaddr)), PAGE_SIZE);
	coremap[0].proc_addr = addr_new_start;
	coremap[0].alloc_num = 0;
	
	//update the numofFrame that does not include the coremap size
	numofFrame = (addr_end - addr_new_start) / PAGE_SIZE;

	//finish the coremap init
	paddr_t acc_addr = addr_new_start;
	for (int i = 1; i < numofFrame; i++) {
		acc_addr = acc_addr + PAGE_SIZE;
		coremap[i].proc_addr = acc_addr;
		coremap[i].alloc_num = 0;
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
	
	int index = 0;
	int counter = 0;
	int int_npages = (int) npages;
	if (kern_call == false) { //not the kern_call
		for (int i = 0; i < numofFrame; i++) {
			if (coremap[i].alloc_num == 0) { //not in use
				counter = counter + 1;
				if (counter == int_npages) {
					index = i;
					break;
				}
			} else { //current use
				counter = 0;
				if (counter == int_npages) {
					index = i;
					break;
				}
			}
		}

		//find the starting index
		int found = index - int_npages + 1;
		//update the coremap with given index with one allocation
		if (int_npages == 1) {
			coremap[found].alloc_num = int_npages;
		}
		//offset + index * pagesize
		addr = addr_new_start + found * PAGE_SIZE;
		/*
		int tt = 0;
		for (int i =0; i < numofFrame; i++) {
			if (coremap[i].proc_addr == test) {
				tt = i;
				break;
			}
		}
		*/
		//kprintf("tt %d\n",tt);
		//kprintf("ff %d\n",found);
		//addr = coremap[found].proc_addr;
		//addr = test;
		//KASSERT(test==addr);
		
		//update the rest core map with more than 1 allocation
		if (int_npages > 1) {
			int value = int_npages;
			for (int i = 0; i < int_npages; i++) {
				coremap[found+i].alloc_num = value;
				value--;
			}
		}

	} else {
		addr = ram_stealmem(npages);
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
		/*
		int init_state = 0;
		for (int i = 0; i < numofFrame; i++) {
			if (coremap[i].proc_addr == addr) {
				init_state = i;
				break;
			}
		}
		*/

		//transfer the addr to padd and find the starting index
		int init_state = (KVADDR_TO_PADDR(addr) - addr_new_start) / PAGE_SIZE;
		//KASSERT(test_index==init_state);
		//kprintf("one process down");
		//free that address and any contiguous frames
		int pagenum = coremap[init_state].alloc_num;
		for (int i = 0; i < pagenum; i++) {
			coremap[init_state + i].alloc_num = 0;
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
	//KASSERT(as->2as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	//KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	//KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	//KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	//KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	//KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	//stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		vaddr_t pt1_index = (faultaddress - vbase1) / PAGE_SIZE;
		paddr = as->as_pagetable_base1[pt1_index];
		#if OPT_A3
			if_read_only = true;
		#endif
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		vaddr_t pt2_index = (faultaddress - vbase2) / PAGE_SIZE;
		paddr = as->as_pagetable_base2[pt2_index];
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		vaddr_t pts_index = (faultaddress - stackbase) / PAGE_SIZE;
		paddr = as->as_pagetable_stackpbase[pts_index];
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
	//as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	//as->as_pbase2 = 0;
	as->as_npages2 = 0;
	//as->as_stackpbase = 0;
	#if OPT_A3
	as->hasloaded = false;

	//init pagetable pointers
	as->as_pagetable_base1 = NULL;
	as->as_pagetable_base2 = NULL;
	as->as_pagetable_stackpbase = NULL;
	#endif
	return as;
}

void
as_destroy(struct addrspace *as)
{
#if OPT_A3
	for (size_t i = 0; i < as->as_npages1; i++) {
		free_kpages(PADDR_TO_KVADDR(as->as_pagetable_base1[i]));
	}
	for (size_t i = 0; i < as->as_npages2; i++) {
		free_kpages(PADDR_TO_KVADDR(as->as_pagetable_base2[i]));
	}
	for (size_t i = 0; i < DUMBVM_STACKPAGES; i++) {
		free_kpages(PADDR_TO_KVADDR(as->as_pagetable_stackpbase[i]));
	}

	free_kpages((vaddr_t) as->as_pagetable_base1);
	free_kpages((vaddr_t) as->as_pagetable_base2);
	free_kpages((vaddr_t) as->as_pagetable_stackpbase);
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

	KASSERT(as->as_pagetable_base1 == NULL);
	KASSERT(as->as_pagetable_base2 == NULL);
	KASSERT(as->as_pagetable_stackpbase == NULL);

	//create pagetable1
	as->as_pagetable_base1 = kmalloc(sizeof(paddr_t) * as->as_npages1);
	if (as->as_pagetable_base1 == NULL) {
		return ENOMEM;
	}

	as->as_pagetable_base2 = kmalloc(sizeof(paddr_t) * as->as_npages2);
	if (as->as_pagetable_base2 == NULL) {
		return ENOMEM;
	}

	as->as_pagetable_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);
	if (as->as_pagetable_stackpbase == NULL) {
		return ENOMEM;
	}


	//npages 1 init
	for(size_t i = 0; i < as->as_npages1; i++) {
		as->as_pagetable_base1[i] = getppages(1);
		as_zero_region(as->as_pagetable_base1[i], 1);
	}
	
	for(size_t i = 0; i < as->as_npages2; i++) {
		as->as_pagetable_base2[i] = getppages(1);
		as_zero_region(as->as_pagetable_base2[i], 1);
	}

	for(size_t i = 0; i < DUMBVM_STACKPAGES; i++) {
		as->as_pagetable_stackpbase[i] = getppages(1);
		as_zero_region(as->as_pagetable_stackpbase[i], 1);
	}

	//as_zero_region(as->as_pbase1, as->as_npages1);
	//as_zero_region(as->as_pbase2, as->as_npages2);
	//as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	#if OPT_A3
		as->hasloaded = true;
		for (int i = 0; i < NUM_TLB; i++) {
			tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
		}
	#else
	(void)as;
	#endif
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_pagetable_stackpbase != NULL);

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

	KASSERT(new->as_pagetable_base1 != NULL);
	KASSERT(new->as_pagetable_base2 != NULL);
	KASSERT(new->as_pagetable_stackpbase != NULL);

	for (size_t i = 0; i < old->as_npages1; i++) {
	memmove((void *)PADDR_TO_KVADDR(new->as_pagetable_base1[i]),
		(const void *)PADDR_TO_KVADDR(old->as_pagetable_base1[i]),
		PAGE_SIZE);
	}

	for (size_t i = 0; i < old->as_npages2; i++) {
	memmove((void *)PADDR_TO_KVADDR(new->as_pagetable_base2[i]),
		(const void *)PADDR_TO_KVADDR(old->as_pagetable_base2[i]),
		PAGE_SIZE);
	}

	for (size_t i = 0; i < DUMBVM_STACKPAGES; i++) {
	memmove((void *)PADDR_TO_KVADDR(new->as_pagetable_stackpbase[i]),
		(const void *)PADDR_TO_KVADDR(old->as_pagetable_stackpbase[i]),
		PAGE_SIZE);
	}
	*ret = new;
	return 0;
}
