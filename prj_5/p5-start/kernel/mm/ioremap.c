#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // map phys_addr to a virtual address
    uintptr_t va = io_base;
    io_base += size;
    //fill the PTE
    uintptr_t pgdir = PGDIR_PA+8+0xffffffc000000000;
    alloc_page_helper(va, pgdir);
    local_flush_tlb_all();
    // then return the virtual address
    return (void *)va;
} 

void iounmap(void *io_addr)
{
    // TODO: a very naive iounmap() is OK
    // maybe no one would call this function?
    // *pte = 0;
}
