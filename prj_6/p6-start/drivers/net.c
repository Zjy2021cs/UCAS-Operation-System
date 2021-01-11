#include <net.h>
#include <os/string.h>
#include <screen.h>
#include <emacps/xemacps_example.h>
#include <emacps/xemacps.h>

#include <os/sched.h>
#include <os/mm.h>

EthernetFrame rx_buffers[RXBD_CNT];
EthernetFrame tx_buffer;
uint32_t rx_len[RXBD_CNT];
 
int net_poll_mode;

volatile int rx_curr = 0, rx_tail = 0;

long do_net_recv(uintptr_t addr, size_t length, int num_packet, size_t* frLength)
{
    // TODO: 
    // receive packet by calling network driver's function
    long status; 
    status = EmacPsRecv(&EmacPsInstance, (EthernetFrame *)rx_buffers, num_packet);  //set RX descripter and enable mac
    
    // wait until you receive enough packets(`num_packet`).
    status = EmacPsWaitRecv(&EmacPsInstance, num_packet, rx_len); //task1:polling
    //copy the buffer_data and length_array into user_space
    kmemcpy((uint8_t *)addr, (uint8_t *)rx_buffers, length);
    kmemcpy((uint8_t *)frLength, (uint8_t *)rx_len, RXBD_CNT*sizeof(uint32_t));
    
    // maybe you need to call drivers' receive function multiple times ?
    return status;
}

void do_net_send(uintptr_t addr, size_t length)
{
    // TODO:
    //copy buffer from user_space to kernel
    kmemcpy((uint8_t *)tx_buffer, (uint8_t *)addr, sizeof(EthernetFrame));
    // send all packet (every call do_net_send() sends one packet)
    long status;
    status = EmacPsSend(&EmacPsInstance, (EthernetFrame *)tx_buffer, length); //set TX descripter and enable mac
    // wait until dma finish sending a packet.
    status = EmacPsWaitSend(&EmacPsInstance);   //task1:polling
    // maybe you need to call drivers' send function multiple times ?
}

void do_net_irq_mode(int mode)
{
    // TODO:
    // turn on/off network driver's interrupt mode
}
