#include <os/list.h>
#include <os/mm.h>
#include <os/sched.h>
#include <pgtable.h>
#include <os/string.h>
#include <assert.h>

ptr_t memCurr = FREEMEM;

static LIST_HEAD(freePageList);

//分配numPage页空间，返回低地址，高地址存放在memCurr中
ptr_t allocPage()
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(memCurr, PAGE_SIZE);
    memCurr = ret + PAGE_SIZE;
    return ret;
}

void freePage(ptr_t baseAddr)
{
    // TODO:
}

void *kmalloc(size_t size)
{
    ptr_t ret = ROUND(memCurr, 4);
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
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page.
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO:
}
