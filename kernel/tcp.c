#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"

// Buffer for received data
char buffer[1024];

// Data to send to the server
const char *request = "GET / HTTP/1.1\r\nHost: 192.168.1.100\r\n\r\n";

void tcp_connected_callback(void *arg, struct tcp_pcb *tcp_pcb, err_t err)
{
    // Check if the connection was successful
    if (err == ERR_OK)
    {
        // Send the request using the tcp_write() function
        tcp_write(tcp_pcb, request, strlen(request), TCP_WRITE_FLAG_COPY);

        // Set the receive callback for the TCP control block using the tcp_recv() function
        tcp_recv(tcp_pcb, tcp_recv_callback);
    }
    else
    {
        // Print an error message
        printf("Failed to connect to the server: %d\n", err);

        // Close the TCP control block using the tcp_close() function
        tcp_close(tcp_pcb);
    }
}

void tcp_recv_callback(void *arg, struct tcp_pcb *tcp_pcb, struct pbuf *p, err_t err)
{
    // Check if there is data to process
    if (p != NULL)
    {
        // Copy the received data into the buffer
        int len = pbuf_copy_partial(p, buffer, sizeof(buffer), 0);

        // Process the received data...

        // Free the pbuf using the pbuf_free() function
        pbuf_free(p);
    }
    else
    {
        // Print a message
        printf("No more data from the server.\n");

        // Close the TCP control block using the tcp_close() function
        tcp_close(tcp_pcb);
    }
}


int main()
{
    // TCP control block and local address
    struct tcp_pcb *tcp_pcb;
    struct ip_addr_t local_addr;

    // Server address and port
    struct ip_addr_t server_addr;
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
    tcp_connect(tcp_pcb, &server_addr, server_port, tcp_connected_callback);

    // Wait for the connection to be established
    while(!tcp_connected(tcp_pcb)) {
        // Feed incoming packets to lwIP and check timeouts
        tcp_tmr();
        sys_check_timeouts();
    }

    // Send data to the server using the tcp_write() function
    tcp_write(tcp_pcb, data, strlen(data), TCP_WRITE_FLAG_COPY);

    // Wait for the data to be sent
    while(tcp_sndbuf(tcp_pcb) < strlen(data)) {
        // Feed incoming packets to lwIP and check timeouts
        tcp_tmr();
        sys_check_timeouts();
    }

    // Receive data from the server using the tcp_recv() function
    tcp_recv(tcp_pcb, tcp_recv_callback);

    // Wait for the connection to be closed
    while(tcp_connected(tcp_pcb)) {
        // Feed incoming packets to lwIP and check timeouts
        tcp_tmr();
        sys_check_timeouts();
    }

    // Close the connection using the tcp_close() function
    tcp_close(tcp_pcb);

    return 0;
}