// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pte = ((pte_t *) vpt)[VPN(addr)];
	if(!((err & FEC_WR) != 0 && (pte & PTE_COW) != 0)){
		panic("pgfault: bad page fault at %08x err %8x", addr, err);
		return ;
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	
	// LAB 4: Your code here.
	if((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0){
		panic("pgfault: page alloc for PFTEMP error");
	}
	memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
	if((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P | PTE_U | PTE_W)) < 0){
		panic("pgfault: page map for PFTEMP error");
	}
	sys_page_unmap(0, PFTEMP);
	
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why mark ours copy-on-write again
// if it was already copy-on-write?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void *addr = (void *)(pn << PGSHIFT);
	pte_t pte;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	if(vpt[pn] & (PTE_W | PTE_COW)){
		if((r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U | PTE_COW)) < 0){
			return r;
		}
		if((r = sys_page_map(0, addr, 0, addr, PTE_P | PTE_U | PTE_COW)) < 0){
			return r;
		}
	}
	else{
		if((r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U)) < 0){
			return r;
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" and the user exception stack in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	envid_t envid;
	uintptr_t va;
	int r;

	set_pgfault_handler(pgfault);

	envid = sys_exofork();
	if(envid < 0){
		panic("fork: unable to create new env");
	}
	if(envid == 0){
		env = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	
	int i, j;
	for(i = 0; i * PTSIZE < UTOP; i++){
		if(((pte_t *)vpd)[i] & PTE_P){
			for(j = 0; j * PGSIZE + i * PTSIZE < UTOP && j < NPTENTRIES; j++){
				if(j * PGSIZE + i * PTSIZE == UXSTACKTOP - PGSIZE){
					continue;
				}
				pte_t p = ((pte_t *) vpt)[i * NPTENTRIES + j];
				if((p & PTE_P) && (p & PTE_U)){
					if ((r = duppage(envid, i * NPTENTRIES + j)) < 0){
						panic("fork : duppage error: %e", r);
					}
				}
			}
		}
	}
	
	if((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0){
		panic("[%08x] fork : sys_page_alloc error : %e.\n", env->env_id, r);
	}

	extern void _pgfault_upcall(void);
	if((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0){
    		panic("[%08x] fork : sys_env_set_pgfault_upcall error : %e.\n", env->env_id, r);
	} 
	if((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0){
    		panic("[%08x] fork : sys_env_set_status error : %e", env->env_id, r);
	}
	
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
