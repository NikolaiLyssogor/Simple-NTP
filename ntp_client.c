#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

#define SERVERADDR "utcnist2.colorado.edu"
#define SERVERPORT "123"
#define PACKETSIZE = 48 // Size of our NTP packets

typedef struct {

    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;

    // Not needed for assignment
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;

    // Split into integer and fraction part
    uint32_t ref_ts_int;
    uint32_t ref_ts_frac;

    uint32_t org_ts_int;
    uint32_t org_ts_frac;

    uint32_t rec_ts_int;
    uint32_t rec_ts_frac;

    uint32_t trans_ts_int;
    uint32_t trains_ts_frac;
    
} ntp_packet;

int main() {
    int sockfd, bytes_recvd;

    // Set up the address info struct
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP datagram socket

    // servinfo now points to linked list of addrinfos
    int status = getaddrinfo(SERVERADDR, SERVERPORT, &hints, &servinfo);
    if(status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 2;
    }

    // Loop through servinfo and connect to first option
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        // Connect to the server (optional, since we're using UDP)
        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break; // found file descriptor
    }

    if(p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    // Prepare the NTP packet and send it
    ntp_packet packet;
    memset(&packet, 0, sizeof(packet)); // zero out the packet
    packet.li_vn_mode = 0x23; //
    int bytes_sent = send(sockfd, &packet, sizeof(packet), 0);
    freeaddrinfo(servinfo);

    printf("bytes sent: %d\n", bytes_sent);

    // Get server timestamps back
    if ((bytes_recvd = recv(sockfd, (char*) &packet, sizeof(ntp_packet), 0)) == -1) {
        perror("recv");
        exit(1);
    }

    printf("bytest received: %d\n", bytes_recvd);



    return 0;
}