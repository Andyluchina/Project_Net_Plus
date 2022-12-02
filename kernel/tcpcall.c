#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
// #include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/tcp.h"

#define CLOUDFAREIP "1.1.1.1"

// struct netif netif;
err_t
tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
  printf("TCP Connected!\n");
  return ERR_OK;
}

int
tcpallinone(int i)
{
  struct tcp_pcb* pcb = tcp_new();
  printf("initializing pcb\n");
  ip_addr_t ip;
  
  if (ipaddr_aton(CLOUDFAREIP, &ip) == 0) {
    printf("IP Conversion Failed\n");
    return -1;
  }
  u16_t port = 443;
  tcp_connect(pcb, &ip, port, tcp_connected);

  if(tcp_close(pcb) == ERR_OK) {
    printf("pcb closed\n");
    return 0;
  } else {
    return -1;
  };


}
