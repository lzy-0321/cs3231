#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <spl.h>
#include <proc.h>

uint32_t hash_func(struct addrspace *as, vaddr_t vaddr)
{
    return (((uint32_t)as) ^ (vaddr >> 12)) % HASH_TABLE_SIZE;
}


struct PTE **allocate_and_initialize_hash_table(void)
{
    struct PTE **hash_table = kmalloc(HASH_TABLE_SIZE * sizeof(struct PTE *));
    if (hash_table == NULL) {
        return NULL;
    }

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_table[i] = NULL;
    }

    return hash_table;
}

struct PTE *pte_find(struct addrspace *as, vaddr_t vaddr)
{
    uint32_t index = hash_func(as, vaddr);
    struct PTE *pte = as->hash_table[index];

    while (pte) {
        if (pte->VPN == vaddr) {
            return pte;
        }
        pte = pte->hash_next;
    }

    return NULL;
}

void pte_insert(struct addrspace *as, struct PTE *new_pte)
{
    uint32_t index = hash_func(as, new_pte->VPN);
    new_pte->hash_next = as->hash_table[index];
    as->hash_table[index] = new_pte;
}

void pte_remove(struct addrspace *as, vaddr_t vaddr)
{
    uint32_t index = hash_func(as, vaddr);
    struct PTE *prev = NULL;
    struct PTE *cur = as->hash_table[index];

    while (cur) {
        if (cur->VPN == vaddr) {
            if (prev) {
                prev->hash_next = cur->hash_next;
            } else {
                as->hash_table[index] = cur->hash_next;
            }
            kfree(cur);
            break;
        }
        prev = cur;
        cur = cur->hash_next;
    }
}

void delete_hash_table(struct addrspace *as)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        struct PTE *pte = as->hash_table[i];
        while (pte) {
            struct PTE *next = pte->hash_next;
            kfree(pte);
            pte = next;
        }
    }
}

void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    /*check does faulttype is READONLY*/

    if (faulttype == VM_FAULT_READONLY){
        return EFAULT;
    } 
    
    uint32_t vpn = faultaddress >> 12; 
    // uint32_t offset = (faultaddress << 20) >> 20;

    /*to get current address space struct*/
    struct addrspace *as = proc_getas(); 

    /*to get index to find entry in page table*/
    // uint32_t index = hash_func(as, faultaddress);

    /*lookup PT*/
    struct PTE *valid_pte = pte_find(as, faultaddress);

    /*If there is no vaid entry in pt then look up region
      if there is a valid entry in pt then load TLB*/
    if (valid_pte == NULL) {
        struct region *region = as->region;
        int valid_region = 0;

        /*look up region*/
        while (region != NULL) {
            if(region->vaddr == faultaddress) {
                valid_region = 1;
                break;
            }
            region = region->next;
        }
        if (valid_region == 0) {
            return EFAULT;
        } else {
            /*allocate frame*/
            paddr_t paddr = alloc_kpages(1);
            if (paddr == 0) {
		        return 0;
	        }
            /*create new pte then insert it into PTE*/
            valid_pte = kmalloc(sizeof(struct PTE));
            if (valid_pte == NULL) {
                return ENOMEM;
            }
            valid_pte->VPN = vpn;
            valid_pte->PFN = paddr >> 12;
            pte_insert(as, valid_pte);
            /*load TLB*/
            uint32_t EntryHi = faultaddress;
            uint32_t EntryLo = paddr;
            tlb_random(EntryHi, EntryLo);
        }
    }
    else {
        /*Load TLB*/
        uint32_t EntryHi = faultaddress & PAGE_FRAME;
        uint32_t EntryLo = valid_pte->PFN << 12;
        tlb_random(EntryHi, EntryLo);
    }
    return 0;

}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}


