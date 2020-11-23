#include <asm/sbidef.h>
#include <asm/sbiasm.h>
#include <csr.h>

// 1. kernel address (move kernel to here ~)
.equ kernel, 0x50201000 

// 2. kernel main address (jmp here to start kernel main!)
.equ kernel_main, 0x50201000

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
	la       a3, os_size_loc
	lh       a1, (a3)            #num_of_blocks
	li       a2, 1               #block_id
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
	li t0, SR_SIE  //??SR_SPP | SR_SPIE??
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
