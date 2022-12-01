//
// test program for lwip tcp.
//

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

void test0();

int
main(int argc, char *argv[])
{
  test0();
  exit(0);
}

// a simple test program for lwip tcp.
void
test0()
{
  printf("test0 start\n");
   // TCP control block and local address
    struct tcp_pcb *tcp_pcb;
    struct ip_addr local_addr;

    // Server address and port
    struct ip_addr server_addr;
    const char *server_ip = "192.168.1.100";
    const int server_port = 80;

    // Data to send to the server
    const char *data = "Hello from lwIP!";

    // Buffer for received data
    char buffer[1024];

    // Create a new TCP control block using the tcp_new() function
    tcp_pcb = tcp_new();

    // Bind the control block to a local address and port using the tcp_bind() function
    local_addr.addr = htonl(INADDR_ANY);
    tcp_bind(tcp_pcb, &local_addr, 0);

    // Set the server address and port
    server_addr.addr = inet_addr(server_ip);
}
