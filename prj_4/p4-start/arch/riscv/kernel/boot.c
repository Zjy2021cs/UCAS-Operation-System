/* RISC-V kernel boot stage */
#include <context.h>
#include <os/elf.h>
#include <pgtable.h>
#include <sbi.h>
#include <os/mm.h>

typedef void (*kernel_entry_t)(unsigned long);

extern unsigned char _elf_main[];
extern unsigned _length_main; 
uintptr_t boot_memCurr = PGDIR_PA + NORMAL_PAGE_SIZE;   //begin from 4KB after pgdir

/********* setup memory mapping ***********/
//分配4KB页空间，返回低地址，高地址存放在boot_memCurr中
uintptr_t alloc_page()
{
    // TODO: alloc pages for page table entries
    uintptr_t alloc = boot_memCurr;
    boot_memCurr = alloc + NORMAL_PAGE_SIZE;
    return alloc;
}

// using 2MB large page   
void map_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
    // TODO: map va to pa
    uint64_t second_pgdir = alloc_page();
    uint64_t pgdir_ppn = (second_pgdir/4096) << 10;
    *pgdir = pgdir_ppn | _PAGE_PRESENT | _PAGE_GLOBAL | _PAGE_ACCESSED | _PAGE_DIRTY;
    uint64_t vpn1 = 0;
    uint64_t second_ppn;
    while(vpn1>=0 && vpn1<=0x1ff){
        second_ppn = (pa/4096) << 10;
        *(PTE *)second_pgdir = second_ppn | _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_GLOBAL | _PAGE_ACCESSED | _PAGE_DIRTY;
        vpn1 += 1;
        second_pgdir += 8;
        pa += LARGE_PAGE_SIZE;   //2MB
    }
}

void enable_vm()
{
    // TODO: write satp to enable paging
    // remember to flush TLB
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA/4096);
    local_flush_tlb_all();
}

/* Sv-39 mode
 * 0x0000_0000_0000_0000-0x0000_003f_ffff_ffff is for user mode
 * 0xffff_ffc0_0000_0000-0xffff_ffff_ffff_ffff is for kernel mode
 */
void setup_vm()
{
    // TODO:
    clear_pgdir(PGDIR_PA);
    //temporary mapping 2MB for 0x50201000; 
    uint64_t pa = 0x50200000;              //vpn2=1;vpn1=129; 
    uintptr_t pgdir = PGDIR_PA+8;
    uint64_t second_pgdir = alloc_page() + 129*8;
    *(PTE *)pgdir = ((second_pgdir/4096)<<10) | _PAGE_PRESENT | _PAGE_GLOBAL | _PAGE_ACCESSED | _PAGE_DIRTY;
    *(PTE *)second_pgdir = ((pa/4096)<<10) | _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_GLOBAL | _PAGE_ACCESSED | _PAGE_DIRTY;
    // map kernel virtual address(kva) to kernel physical
    // address(kpa) kva = kpa + 0xffff_ffc0_0000_0000 use 2MB page,
    // map all physical memory            
    uint64_t va = 0xffffffc000000000;
    pgdir = PGDIR_PA + 0x800;    //kernel:+2KB
    while(va<=0xffffffffffffffff && va>=0xffffffc000000000){
        pa = (va << 26) >> 26;
        map_page(va, pa, (PTE *)pgdir);
        va += 0x40000000;
        pgdir += 8;
    }
    // enable virtual memory
    enable_vm();
}

uintptr_t directmap(uintptr_t kva, uintptr_t pgdir)
{
    // ignore pgdir
    return kva;
}

kernel_entry_t start_kernel = NULL;

/*********** start here **************/
int boot_kernel(unsigned long mhartid)
{
    if (mhartid == 0) {
        setup_vm();
        // load kernel
        start_kernel = 
            (kernel_entry_t)load_elf(_elf_main, _length_main,
                                     PGDIR_PA, directmap);
    } else {
        // TODO: what should we do for other cores?
        /* set SATP to enable sv39_mode */
        set_satp(SATP_MODE_SV39, 0, PGDIR_PA/4096);
    }
    start_kernel(mhartid);
    return 0;
}
