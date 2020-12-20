#include <os/list.h>
#include <os/mm.h>
#include <os/sched.h>
#include <pgtable.h>
#include <os/string.h>
#include <assert.h>

ptr_t memCurr = FREEMEM;

//static LIST_HEAD(freePageList);
ptr_t freePageList = (ptr_t)&freePageList;

//分配numPage页空间，返回低地址，高地址存放在memCurr中
ptr_t allocPage()
{
    // align PAGE_SIZE
    ptr_t ret;
    if(freePageList==(ptr_t)&freePageList){
        ret = ROUND(memCurr, PAGE_SIZE);
        memCurr = ret + PAGE_SIZE;
    }else{
        ret = freePageList;
        freePageList = *(long *)(ret-8);
    }
    return ret;
}

//recycle a free page
void freePage(ptr_t baseAddr)
{
    /**(long *)(baseAddr-8) = freePageList;
    freePageList = baseAddr;*/
    //list_add(list_node_t *node, &freePageList)
}

void *kmalloc(size_t size)
{
    ptr_t ret = ROUND(memCurr, size);
    memCurr = ret + size;
    return (void*)ret;
}

uintptr_t shm_page_get(int key)
{
    // TODO(c-core):
}

void shm_page_dt(uintptr_t addr) 
{
    // TODO(c-core):
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO:
    //copy from PGDIR_PA + 2KB to user_pgdir + 2KB; once 1Byte
    uint32_t len = 0x800;
    kmemcpy((uint8_t *)(dest_pgdir+0x800), (uint8_t *)(src_pgdir+0x800), len);
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page.
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    uint64_t vpn2 = va >> 30;
    uint64_t vpn1 = (va << 34) >> 55;
    uint64_t vpn0 = (va << 43) >> 55;
    uint64_t offset = (va << 52) >> 52;
    uint64_t second_pgdir;
    uint64_t third_pgdir;
    uint64_t pgdir_entry = *(PTE *)(pgdir+vpn2*8);

    if((pgdir_entry%2)==0){          //invalid
        second_pgdir = allocPage();  //virtual addr
        uint64_t pgdir_ppn = ((second_pgdir-0xffffffc000000000)/4096) << 10;  //physical addr
        *(PTE *)(pgdir+vpn2*8) = pgdir_ppn | _PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY;
    }else{
        second_pgdir = (pgdir_entry >> 10) * 4096 + 0xffffffc000000000; //virtual addr
    }

    uint64_t pgdir2_entry = *(PTE *)(second_pgdir+vpn1*8);
    if((pgdir2_entry%2)==0){         //invalid
        third_pgdir = allocPage();
        uint64_t second_ppn = ((third_pgdir-0xffffffc000000000)/4096) << 10;
        *(PTE *)(second_pgdir+vpn1*8) = second_ppn | _PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY;
    }else{
        third_pgdir = (pgdir2_entry >> 10) * 4096 + 0xffffffc000000000;
    }

    uint64_t pa = allocPage();       
    uint64_t third_ppn = ((pa-0xffffffc000000000)/4096) << 10;
    *(PTE *)(third_pgdir+vpn0*8) = third_ppn | _PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY;    
    return pa;
}
