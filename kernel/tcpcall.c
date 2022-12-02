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
// #include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/tcp.h"
#include "lwip/tcpbase.h"

#define CLOUDFAREIP "67.159.94.97"
const char *request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
char buffer[1024];

struct spinlock socketlock;

// struct netif netif;
static err_t
tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
  printf("TCP Connected Function call!\n");
  *(int *)arg = 0;
  wakeup(arg);
  return ERR_OK;
}

static void
tcp_error(void* arg, err_t err) {
  printf("TCP Unsuccessful!\n");
  *(int *)arg = 0;
  wakeup(arg);
}

err_t
tcp_recv_callback(void *arg, struct tcp_pcb *tcp_pcb, struct pbuf *p, err_t err)
{
    // Check if there is data in the buffer
    printf("Calling recv callback\n");
    if (p != NULL)
    {
        // Process the received data...
        int length = p->len;
        pbuf_copy_partial(p, buffer, length, 0);
        printf("Received %d bytes:\n", length);
        printf("%s\n", buffer);

        // Free the buffer
        pbuf_free(p);
    }

    // Check if the transmission is complete
    if (err == ERR_OK)
    {
        // Transmission is complete, print a message and close the control block
        printf("Transmission complete\n");
        tcp_close(tcp_pcb);
    }
    else
    {
        // Transmission failed, print an error message and close the control block
        printf("Error receiving data from the server: %d\n", err);
        tcp_close(tcp_pcb);
    }
    return err;
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
  u16_t port = 8877;
  // tcp_err(pcb, tcp_connect);
  // tcp_recv(pcb, tcp_connect);
  // tcp_sent(pcb, tcp_connect);
  acquire(&socketlock);

  tcp_err_fn err_fn = tcp_error;
  tcp_connected_fn suc_fn = tcp_connected;

  int success = 1;
  tcp_arg(pcb, &success);
  tcp_err(pcb, err_fn);
  tcp_recv(pcb, tcp_recv_callback);
  err_t res = tcp_connect(pcb, &ip, port, suc_fn);
  if (res == ERR_OK) {
    printf("Connect Success\n");
  }

  sleep(&success, &socketlock);
  // Send the request using the tcp_write() function
  err_t write_succ = tcp_write(pcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
  if (write_succ == ERR_OK) {
    printf("TCP Write done\n");
  }

  release(&socketlock);

  // if(tcp_close(pcb) == ERR_OK) {
  //   printf("pcb closed\n");
  //   release(&socketlock);
  //   return 0;
  // } else {
  //   release(&socketlock);
  //   return -1;
  // };
  return 0;
}
