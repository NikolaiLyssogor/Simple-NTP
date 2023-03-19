#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "ntppacket.h"

#define PORT "4123"
#define PACKETSIZE 48 // size of our NTP packets
#define NTP_EPOCH_OFFSET 2208988800UL // seconds between 1/1/1900 (NTP epoch) and 1/1/1970 (Unix epoch)

void *get_in_addr(struct sockaddr *sa) {
    // Get IPv4 sockaddr
    return &(((struct sockaddr_in*)sa)->sin_addr);
}

int main(void) {

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    int rv;
    int yes = 1;
    ntp_packet packet;
    int bytes_recvd;
    struct timeval recv_tv, trans_tv; // Time request arrives and is returned

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through servinfo and bind to first possible
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        // lose the pesky "Address already in use" error message
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(yes)) == -1) {
                perror("setsockopt");
                exit(1);
            }

        // Bind to address
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    printf("server: waiting to recv...\n");

    while(1) {
        addr_len = sizeof(client_addr);

        // Zero out the NTP packet variable before recv
        memset(&packet, 0, sizeof(ntp_packet));

        // Receive NTP packet from client. Discard if it's the wrong size
        if((bytes_recvd = recvfrom(sockfd, &packet, sizeof(packet), 0,
                (struct sockaddr *)&client_addr, &addr_len)) != PACKETSIZE) {
            perror("recv");
        }
        else { // Stamp the packet
            gettimeofday(&recv_tv, NULL);

            // printf("server: got packet from %s\n",
            //     inet_ntop(client_addr.ss_family,
            //         get_in_addr((struct sockaddr *) &client_addr),
            //         s, sizeof(s)));

            packet.recv_ts_int = htonl(timeval_to_ntp_seconds(recv_tv.tv_sec));
            packet.recv_ts_frac = htonl(timeval_to_ntp_frac(recv_tv.tv_usec));

            packet.li_vn_mode = 0x24; // 00 100 100
            packet.stratum = 0x2; 
            packet.poll = 0x10;
            // packet.precision = ???;

            gettimeofday(&trans_tv, NULL);
            packet.trans_ts_int = htonl(timeval_to_ntp_seconds(trans_tv.tv_sec));
            packet.trans_ts_frac = htonl(timeval_to_ntp_frac(trans_tv.tv_usec));

            // Dish it back over to the client, don't worry about dropped bits
            sendto(sockfd, &packet, sizeof(ntp_packet), 0, &client_addr, addr_len);
        }
    }

    close(sockfd);
    return 0;
}