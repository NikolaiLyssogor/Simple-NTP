#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#define SERVERADDR "utcnist2.colorado.edu"
#define SERVERPORT "123"
#define PACKETSIZE 48 // size of our NTP packets
#define NTP_EPOCH_OFFSET 2208988800UL // seconds between 1/1/1900 (NTP epoch) and 1/1/1970 (Unix epoch)

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

    uint32_t recv_ts_int;
    uint32_t recv_ts_frac;

    uint32_t trans_ts_int;
    uint32_t trans_ts_frac;
    
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
    packet.li_vn_mode = 0x23; // li = 0, vn = 4, mode = 3
    int bytes_sent = send(sockfd, &packet, sizeof(packet), 0);
    freeaddrinfo(servinfo);

    printf("bytes sent: %d\n", bytes_sent);

    // Get server timestamps back
    if ((bytes_recvd = recv(sockfd, (char*) &packet, sizeof(ntp_packet), 0)) == -1) {
        perror("recv");
        exit(1);
    }

    printf("bytest received: %d\n\n", bytes_recvd);

    // Convert from network to host byte order
    packet.stratum = ntohl(packet.stratum);
    packet.poll = ntohl(packet.poll);
    packet.recv_ts_int = ntohl(packet.recv_ts_int);
    packet.recv_ts_frac = ntohl(packet.recv_ts_frac);
    packet.trans_ts_int = ntohl(packet.trans_ts_int);
    packet.trans_ts_frac = ntohl(packet.trans_ts_frac);

    printf("stratum: %u\n", packet.stratum);
    printf("poll: %u\n\n", packet.poll);
    printf("receive seconds: %u\n", packet.recv_ts_int);
    printf("receive fraction: %u\n\n", packet.recv_ts_frac);
    printf("transmit seconds: %u\n", packet.trans_ts_int);
    printf("transmit fraction: %u\n\n", packet.trans_ts_frac);

    // Get local system time (Unix)
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Convert local Unix time to local NTP time
    uint32_t local_ntp_int = tv.tv_sec + NTP_EPOCH_OFFSET;
    printf("local seconds as NTP: %u\n", local_ntp_int);
    uint32_t local_ntp_frac =  (uint32_t)((double)(tv.tv_usec + 1) * (double)(1LL << 32) * 1.0e-6); // https://tickelton.gitlab.io/articles/ntp-timestamps/
    printf("local ms as NTP: %u\n", local_ntp_frac);

    printf("server/local seconds diff: %u\n", packet.recv_ts_int - local_ntp_int);
    printf("server/local ms diff: %u\n", packet.recv_ts_frac - local_ntp_frac);

    return 0;
}