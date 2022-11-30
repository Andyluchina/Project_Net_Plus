struct hostent {
    char *h_name;             /* official name of host */
    char **h_aliases;         /* alias list */
    int h_addrtype;           /* host address type */
    int h_length;             /* length of address */
    char **h_addr_list;       /* list of addresses from name server */
#define h_addr h_addr_list[0] /* address, for backward compatibility */
};

struct in_addr {
    uint32 s_addr;
};

struct sockaddr {
    uint8 sa_len;
    uint8 sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    uint8 sin_len;
    uint8 sin_family;
    uint16 sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

#define AF_UNSPEC 0
#define AF_INET 2
#define AF_INET6 10
#define PF_INET AF_INET
#define PF_INET6 AF_INET6
#define PF_UNSPEC AF_UNSPEC

#ifndef NULL
#define NULL ((void*) 0)
#endif

#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

#define SOL_SOCKET 0xfff
#define SO_ERROR 0x1007
#define SO_TYPE 0x1008


#ifdef htons
#undef htons
#endif
#define htons __builtin_bswap16

#ifdef ntohs
#undef ntohs
#endif
#define ntohs __builtin_bswap16

#ifdef htonl
#undef htonl
#endif
#define htonl __builtin_bswap32

#ifdef ntohl
#undef ntohl
#endif
#define ntohl __builtin_bswap32

#ifndef INADDR_ANY
#define INADDR_ANY 0
#endif