#include "kernel/types.h"
#include "user/user.h"
#include "user/net.h"

#define DEBUGP 1

/* RFC 867: Daytime Protocol */
#define SERVER_HOST "utcnist.colorado.edu"
#define SERVER_PORT 13

char *inet_ntoa_r(struct in_addr in, char *buf, uint32 size)
{
    uint32 s_addr;
    char inv[3];
    char *rp;
    uint8 *ap;
    uint8 rem;
    uint8 n;
    uint8 i;
    int len = 0;

    s_addr = in.s_addr;
    rp = buf;
    ap = (uint8 *)&s_addr;
    for (n = 0; n < 4; n++) {
        i = 0;
        do {
            rem = *ap % (uint8)10;
            *ap /= (uint8)10;
            inv[i++] = (char)('0' + rem);
        } while (*ap);
        while (i--) {
            if (len++ >= size) {
                return NULL;
            }
            *rp++ = inv[i];
        }
        if (len++ >= size) {
            return NULL;
        }
        *rp++ = '.';
        ap++;
    }
    *--rp = 0;
    return buf;
}

char *inet_ntoa(struct in_addr in)
{
    static char buf[16];
    return inet_ntoa_r(in, buf, sizeof(buf));
}


int main(int argc, char **argv)
{
    struct hostent *hp;
    int sockfd, r;
    struct sockaddr_in addr = {
        .sin_family = PF_INET, .sin_port = htons(SERVER_PORT),
    };

    if (DEBUGP) printf("len:%d family:%d port:%d sin_addr:%d sin_zero:%s\n", addr.sin_len, addr.sin_family, addr.sin_port, addr.sin_addr.s_addr, addr.sin_zero);
    hp = gethostbyname(SERVER_HOST);
    if(hp == 0) {
        printf("Err: gethostbyname\n");
        exit(0);
    }
    addr.sin_addr = *(struct in_addr *)(hp->h_addr);
    printf("daytime: %s %s\n", hp->h_name, inet_ntoa(addr.sin_addr));

    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) {
        printf("Err: socket\n");
        exit(0);
    }

    r = connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if(r != 0) {
        printf("Err: connect\n");
        exit(0);
    }

    while (1) {
        char buf[512];
        int n;

        // n = recv(sockfd, buf, sizeof(buf), 0);
        n = read(sockfd, buf, sizeof(buf));
        if (n <= 0)
            break;
        write(1, buf, n);
    }

    close(sockfd);
    return 0;
}
