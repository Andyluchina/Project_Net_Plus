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

struct spinlock socketlock;

// struct netif netif;
static err_t
tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
  printf("TCP Connected!\n");
  *(int *)arg = 0;
  wakeup(arg);
  return ERR_OK;
}

static void
tcp_error(void* arg, err_t err) {
  printf("TCP Unsuccessful!\n");
  *(int *)arg = 1;
  wakeup(arg);
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
  u16_t port = 80;
  // tcp_err(pcb, tcp_connect);
  // tcp_recv(pcb, tcp_connect);
  // tcp_sent(pcb, tcp_connect);
  acquire(&socketlock);

  tcp_err_fn err_fn = tcp_error;
  tcp_connected_fn suc_fn = tcp_connected;

  int success = 1;
  tcp_arg(pcb, &success);
  tcp_err(pcb, err_fn);
  err_t res = tcp_connect(pcb, &ip, port, suc_fn);
  if (res == ERR_OK) {
    printf("Connect Success\n");
  }
  sleep(&success, &socketlock);

  if(tcp_close(pcb) == ERR_OK) {
    printf("pcb closed\n");
    release(&socketlock);
    return 0;
  } else {
    release(&socketlock);
    return -1;
  };
}
