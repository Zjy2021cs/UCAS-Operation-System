#include <asm.h>
#include <csr.h>

//task 4
ENTRY(setup_exception)
  /* TODO: 
   * save exception_handler_entry into STVEC
   * enable global exceptions */
  la t0, exception_handler_entry 
  csrw CSR_STVEC, t0
  li t0, SIE_STIE
  csrw CSR_SIE  , t0
  jr ra
ENDPROC(setup_exception)

/* prj_5:maybe you need it? */
ENTRY(read_sip)
  csrr a0, CSR_SIP
  jr ra
ENDPROC(read_sip)
