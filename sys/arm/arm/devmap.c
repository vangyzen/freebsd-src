/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Routines for mapping device memory.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/devmap.h>

static const struct pmap_devmap *devmap_table;

/*
 * Map all of the static regions in the devmap table, and remember
 * the devmap table so other parts of the kernel can do lookups later.
 */
void
pmap_devmap_bootstrap(vm_offset_t l1pt, const struct pmap_devmap *table)
{
	const struct pmap_devmap *pd;

	devmap_table = table;

	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		pmap_map_chunk(l1pt, pd->pd_va, pd->pd_pa, pd->pd_size,
		    pd->pd_prot,pd->pd_cache);
	}
}

/*
 * Look up the given physical address in the static mapping data and return the
 * corresponding virtual address, or NULL if not found.
 */
void *
arm_devmap_ptov(vm_paddr_t pa, vm_size_t size)
{
	const struct pmap_devmap *pd;

	if (devmap_table == NULL)
		return (NULL);

	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		if (pa >= pd->pd_pa && pa + size <= pd->pd_pa + pd->pd_size)
			return ((void *)(pd->pd_va + (pa - pd->pd_pa)));
	}

	return (NULL);
}

/*
 * Look up the given virtual address in the static mapping data and return the
 * corresponding physical address, or DEVMAP_PADDR_NOTFOUND if not found.
 */
vm_paddr_t
arm_devmap_vtop(void * vpva, vm_size_t size)
{
	const struct pmap_devmap *pd;
	vm_offset_t va;

	if (devmap_table == NULL)
		return (DEVMAP_PADDR_NOTFOUND);

	va = (vm_offset_t)vpva;
	for (pd = devmap_table; pd->pd_size != 0; ++pd) {
		if (va >= pd->pd_va && va + size <= pd->pd_va + pd->pd_size)
			return ((vm_paddr_t)(pd->pd_pa + (va - pd->pd_va)));
	}

	return (DEVMAP_PADDR_NOTFOUND);
}

/*
 * Map a set of physical memory pages into the kernel virtual address space.
 * Return a pointer to where it is mapped. This routine is intended to be used
 * for mapping device memory, NOT real memory.
 */
void *
pmap_mapdev(vm_offset_t pa, vm_size_t size)
{
	vm_offset_t va, tmpva, offset;
	
	offset = pa & PAGE_MASK;
	pa = trunc_page(pa);
	size = round_page(size + offset);
	
	va = kva_alloc(size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");

	for (tmpva = va; size > 0;) {
		pmap_kenter_device(tmpva, pa);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	
	return ((void *)(va + offset));
}

/*
 * Unmap device memory and free the kva space.
 */
void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	vm_offset_t tmpva, offset;
	
	offset = va & PAGE_MASK;
	va = trunc_page(va);
	size = round_page(size + offset);

	for (tmpva = va; size > 0;) {
		pmap_kremove(tmpva);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
	}

	kva_free(va, size);
}

