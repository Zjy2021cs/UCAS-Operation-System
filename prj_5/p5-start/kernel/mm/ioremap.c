#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    /* map phys_addr to a virtual address
    uintptr_t va_begin = io_base;
    io_base += size;
    //fill the PTE
    uintptr_t va = va_begin;
    uint64_t pa = phys_addr; 
    uintptr_t pgdir = PGDIR_PA+0xffffffc000000000;  
    while(va < (va_begin+size)){
        uint64_t vpn2 = (va << 25) >> 55; 
        uint64_t vpn1 = (va << 34) >> 55;
        uint64_t vpn0 = (va << 43) >> 55;
        uint64_t second_pgdir;
        uint64_t third_pgdir;
        uint64_t pgdir_entry = *(PTE *)(pgdir+vpn2*8);
        
        //for kernel page, has filled 2MB map
        second_pgdir = (pgdir_entry >> 10) * 4096 + 0xffffffc000000000; //virtual addr

        uint64_t pgdir2_entry = *(PTE *)(second_pgdir+vpn1*8);
        if((pgdir2_entry%4)==3){         //first map for 3th pagetable
            third_pgdir = allocPage();
            uint64_t second_ppn = ((third_pgdir-0xffffffc000000000)/4096) << 10;
            *(PTE *)(second_pgdir+vpn1*8) = second_ppn | _PAGE_PRESENT | _PAGE_GLOBAL | _PAGE_ACCESSED | _PAGE_DIRTY;
        }else{
            third_pgdir = (pgdir2_entry >> 10) * 4096 + 0xffffffc000000000;
        }

        uint64_t third_ppn = (pa/4096) << 10;
        *(PTE *)(third_pgdir+vpn0*8) = third_ppn | _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_GLOBAL | _PAGE_ACCESSED | _PAGE_DIRTY;    
        
        va += 0x1000;
        pa += 0x1000;
    }

    local_flush_tlb_all();*/
    uint64_t va = phys_addr + 0xffffffc000000000;
    // then return the virtual address
    return (void *)va;
} 

void iounmap(void *io_addr)
{
    // TODO: a very naive iounmap() is OK
    // maybe no one would call this function?
    // *pte = 0;
}
