#include <asm/sbidef.h>
#include <asm/sbiasm.h>
#include <csr.h>  

// 1. kernel address (move kernel to here ~)
.equ kernel, 0x50203000

// 2. kernel main address (jmp here to start kernel main!)
.equ kernel_main, 0x50203000

// 3. os size location (get the size of your OS here)
.equ os_size_loc, 0x502001fc

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// a0 is mhartid, a1 is the pointer of fdt
	move s0,a0
	move s1,a1

	// output a \n
	li a0, 10
	SBI_CALL SBI_CONSOLE_PUTCHAR

	// The core whose mhartid=0 is selected to boot the kernel.
	// Other cores should wait until the kernel's initialization
	// has been finished.
 	move a0, s0
	bnez a0, secondary

	// your code for master core
	 // 1) call BIOS print string "It's a bootloader!"
	la       a0, msg             #*str
	SBI_CALL SBI_CONSOLE_PUTSTR
	 // 2) call BIOS read kernel in SD card and jump to kernel start
	la       a0, kernel          #mem_addr
	li       a1, 64              #num_of_blocks_once
	li       a2, 1               #block_id
	la       t0, os_size_loc
	lh       a3, (t0)            #total_num_of_blocks
	move     t0, a0
	li       t1, 32768           #512B * 64
	ble      a3, a1, read_remain_sector
read_64_sector:
	SBI_CALL SBI_SD_READ
	add      a0, t1, t0
	move     t0, a0
	add      a2, a2, a1 
	sub      a3, a3, a1
	bgt      a3, a1, read_64_sector
read_remain_sector:
	move     a1, a3
	SBI_CALL SBI_SD_READ

	la       t0, kernel_main
	jr		 t0  

secondary:
	/* TODO: for slave core
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	csrw CSR_SIE, zero
  	csrw CSR_SIP, zero

  	la t0, kernel_main        
  	csrw CSR_STVEC, t0
	
	li t0, SIE_SSIE
  	csrw CSR_SIE  , t0
	li t0, SR_SIE         //??SR_SPP | SR_SPIE??
  	csrs CSR_SSTATUS, t0 

wait_for_wakeup:
	wfi
	j wait_for_wakeup

// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"
