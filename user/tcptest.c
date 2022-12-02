#define LWIP_IPV4 1 // enable IPv4 support
#define LWIP_IPV6 1 // enable IPv6 support
#include "lwip/include/lwip/tcp.h"
#include "lwip/include/lwip/ip_addr.h"

// Data to send to the server
const char *request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";

// Buffer for received data
char buffer[1024];

int main()
{
    // TCP control block and local address
    struct tcp_pcb *tcp_pcb;
    struct ip_addr local_addr;

    // Server address and port
    struct ip_addr server_addr;
    const char *server_ip = "192.168.1.100";
    const int server_port = 80;

    // Create a new TCP control block using the tcp_new() function
    tcp_pcb = tcp_new();

    // Bind the control block to a local address and port using the tcp_bind() function
    local_addr.addr = htonl(INADDR_ANY);
    tcp_bind(tcp_pcb, &local_addr, 0);

    // Set the server address and port
    server_addr.addr = inet_addr(server_ip);

    // Set the connect callback for the TCP control block using the tcp_connect() function
    tcp_connect(tcp_pcb, &server_addr, server_port, tcp_connected_callback);

    // Process the received data...

    return 0;
}

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
        // Connection failed, print an error message and close the control block
        printf("Error connecting to the server: %d\n", err);
        tcp_close(tcp_pcb);
    }
}

void tcp_recv_callback(void *arg, struct tcp_pcb *tcp_pcb, struct pbuf *p, err_t err)
{
    // Check if there is data in the buffer
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
}

