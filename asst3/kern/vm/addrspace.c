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
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->region = NULL;
	// init hash table
	// kmalloc will return NULL if there is no enough memory
	
	as->hash_table = allocate_and_initialize_hash_table();
    if (as->hash_table == NULL) {
        kfree(as);
        return NULL;
    }

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
    struct addrspace *newas;

    newas = as_create();
    if (newas==NULL) {
        return ENOMEM;
    }

    // Copy the address space regions
    struct region *old_region = old->region;
    struct region *new_region = NULL;
    struct region *last_region = NULL;
    while (old_region != NULL) {
        new_region = kmalloc(sizeof(struct region));
        if (new_region == NULL) {
            as_destroy(newas);
            return ENOMEM;
        }
        r_copy(old_region, new_region);
        if (last_region == NULL) {
            newas->region = new_region;
        } else {
            last_region->next = new_region;
        }
        last_region = new_region;
        old_region = old_region->next;
    }

    // Copy the page table
    int result = copy_page_table(old, newas);
    if (result != 0) {
        as_destroy(newas);
        return result;
    }

    *ret = newas;
    return 0;
}

void        
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	// delete region
	struct region *region = as->region;
	struct region *next_region;
	while (region != NULL) {
		next_region = region->next;
		r_delete(region);
		region = next_region;
	}
	// delete page table
	delete_hash_table(as);
	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
	int spl = splhigh();
	tlb_flush_all();
	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
	int spl = splhigh();
	tlb_flush_all();
	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	return ENOSYS; /* Unimplemented */

}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

// =====help function=====
//


// create a new region
struct region *r_create(vaddr_t vbase, size_t npages, int readable, int writeable, int executable) {
	struct region *new_region = kmalloc(sizeof(struct region));
	if (new_region == NULL) {
		return NULL;
	}
	new_region->vaddr = vbase;
	new_region->sz = npages;
	new_region->readable = readable;
	new_region->writeable = writeable;
	new_region->executable = executable;
	new_region->next = NULL;
	return new_region;
}

// copy region
void r_copy(struct region *old, struct region *new) {
	new->readable = old->readable;
	new->writeable = old->writeable;
	new->executable = old->executable;
	new->vaddr = old->vaddr;
	new->sz = old->sz;
	new->next = NULL;
}

// copy page table
int copy_page_table(struct addrspace *old, struct addrspace *new)
{
	struct PTE *old_pte, *new_pte;

	for (int i = 0; i < HASH_TABLE_SIZE; i++) {
		old_pte = old->hash_table[i];
		while (old_pte != NULL) {
			new_pte = kmalloc(sizeof(struct PTE));
			if (new_pte == NULL) {
				return ENOMEM;
			}
			memcpy(new_pte, old_pte, sizeof(struct PTE));
			new_pte->hash_next = new->hash_table[i];
			new->hash_table[i] = new_pte;
			old_pte = old_pte->hash_next;
		}
	}

	return 0;
}

// delete region
void r_delete(struct region *region) {
	kfree(region);
}

// write all TLB
void tlb_flush_all(void) {
	for (int i = 0; i < NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
}