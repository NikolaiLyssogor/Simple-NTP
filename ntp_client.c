#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#include "ntppacket.h"

#define SERVERADDR "localhost"
#define SERVERPORT "4123"
#define PACKETSIZE 48 // size of our NTP packets
#define NTP_EPOCH_OFFSET 2208988800UL // seconds between 1/1/1900 (NTP epoch) and 1/1/1970 (Unix epoch)

typedef struct {
    uint64_t delay_usec; // microseconds of delay
    uint64_t offset_usec; // microseconds of offset
} update_value;

uint32_t ntp_to_timeval_frac(uint32_t frac) {
    /*
    Multiply by an appropriate factor to get the required
    subdivision of seconds, e.g. 10^6 for microseconds, and 
    divide by 2^23.
    https://tickelton.gitlab.io/articles/ntp-timestamps/
    */
   return (uint32_t) ((double)frac * 1.0e6 / (double)(1LL << 32));
}

void print_uint32_t_binary(uint32_t value) {
    for (int i = 31; i >= 0; i--) {
        uint32_t bit = (value >> i) & 1;
        printf("%u", bit);
    }

    printf("\n");
}

void compute_delay_and_offset(ntp_packet packet, struct timeval dest_tv, update_value* values) {
    /*
    Packs the pointer to the update_value argument with the delay
    and offset according to the following formula:

    delay = (T4 – T1) – (T3 – T2)
    offset = [(T2 – T1) + (T3 – T4)]

    Here T1 is org_ts, T2 is recv_ts, T3 is trans_ts, and T4 is dest_ts.
    Returns 1 on success and -1 on failure.
    */

    // Convert destination time to NTP format
    int dest_ts_int = timeval_to_ntp_seconds(dest_tv.tv_sec);
    int dest_ts_frac = timeval_to_ntp_frac(dest_tv.tv_usec);

    // Finally, compute delay and offset
    uint32_t delay_int = (dest_ts_int - packet.org_ts_int) - (packet.trans_ts_int - packet.recv_ts_int);
    uint32_t delay_frac = (dest_ts_frac - packet.org_ts_frac) - (packet.trans_ts_frac - packet.recv_ts_frac);
    uint32_t offset_int = 0.5*((packet.recv_ts_int - packet.org_ts_int) + (packet.trans_ts_int - dest_ts_int));
    uint32_t offset_frac = 0.5*((packet.recv_ts_frac - packet.org_ts_frac) + (packet.trans_ts_frac - dest_ts_frac));

    // Convert fraction values to timeval microseconds
    delay_frac = ntp_to_timeval_frac(delay_frac);
    offset_frac = ntp_to_timeval_frac(offset_frac);

    // Pack the update_value passed by reference
    values->delay_usec = delay_int*1000000 + delay_frac;
    values->offset_usec = offset_int*1000000 + offset_frac;
}

update_value send_request() {
    /*
    Makes a request to an NTP server and returns the delay and offset 
    for that request. Uses a new socket for each request. Retransmits
    the packet if the bytes sent or the bytes received is not what is
    expected. Returns error value for all other errors.
    */
   
    // Update value to return in case of error
    update_value error_values = { 0.0, 0.0 };
    update_value success_values;

    int sockfd, bytes_sent, bytes_recvd;
    bytes_sent = 0;
    bytes_recvd = 0;

    while(bytes_sent != PACKETSIZE && bytes_recvd != PACKETSIZE) {
        // Set up the address info struct
        struct addrinfo hints, *servinfo, *p;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // IPv4
        hints.ai_socktype = SOCK_DGRAM; // UDP datagram socket

        // servinfo now points to linked list of addrinfos
        int status = getaddrinfo(SERVERADDR, SERVERPORT, &hints, &servinfo);
        if(status != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            return error_values;
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
            return error_values;
        }

        // Prepare the NTP packet and send it
        ntp_packet packet;
        memset(&packet, 0, sizeof(packet)); // zero out the packet
        packet.li_vn_mode = 0x23; // li = 0 (00), vn = 3 (011), mode = 3 (011)

        // Save originate time for pos-ack later
        struct timeval origin_tv;
        gettimeofday(&origin_tv, NULL); 
        // packet.org_ts_int = htonl(timeval_to_ntp_seconds(origin_tv.tv_sec)); // Not present in recv packet
        // print_uint32_t_binary(packet.org_ts_int);
        // packet.org_ts_int = htonl(timeval_to_ntp_frac(origin_tv.tv_usec));

        // Send the packet to the server
        if((bytes_sent = send(sockfd, &packet, sizeof(packet), 0)) != PACKETSIZE) {
            printf("Only sent %i bytes. Retrying...\n", bytes_sent);
            sleep(4);
            continue;
        }
        freeaddrinfo(servinfo);

        // Get server timestamps back
        if ((bytes_recvd = recv(sockfd, &packet, sizeof(packet), 0)) != PACKETSIZE) {
            printf("Only received %i bytes. Retrying...\n", bytes_recvd);
            sleep(4);
            continue;
        }
        
        close(sockfd);

        // printf("origin seconds post-departure: %u | ", packet.org_ts_int);
        // print_uint32_t_binary(packet.org_ts_int);

        // Generate destination timestamp and convert origin timestamp to NTP format
        struct timeval dest_tv;
        gettimeofday(&dest_tv, NULL);

        // Convert packet fields of interest to host byte order
        packet.recv_ts_int = ntohl(packet.recv_ts_int);
        packet.recv_ts_frac = ntohl(packet.recv_ts_frac);    
        packet.trans_ts_int = ntohl(packet.trans_ts_int);
        packet.trans_ts_frac = ntohl(packet.trans_ts_frac);

        // Pack packet with origin and destination times
        packet.org_ts_int = timeval_to_ntp_seconds(origin_tv.tv_sec);
        packet.org_ts_frac = timeval_to_ntp_frac(origin_tv.tv_usec);

        // Compute update values
        compute_delay_and_offset(packet, dest_tv, &success_values);
    }

    return success_values;
}

void send_request_burst(int burst_num) {
    /*
    Sends a burst of 8 requests, spaces 4 seconds apart (as 
    per the directive on the NIST website). Saves the delay
    and offset to a file. 
    */

    // Open results file in append mode
    FILE *fp;
    fp = fopen("cloud_results.txt", "a");

    // Send burst of 8 requests
    for(int i = 0; i < 8; i++) {
        printf("Sending request %d in burst %d\n", i+1, burst_num+1);

        // Make NTP request
        update_value vals = send_request();

        // Write results to text file
        fprintf(fp, "%llu      %llu\n", vals.delay_usec, vals.offset_usec);
        sleep(4);
    }
    fprintf(fp, "\n");
    fclose(fp);
}

int main(void) {
    // Clear the contents of the results file and write the header
    FILE *fp;
    fp = fopen("cloud_results.txt", "w");
    fprintf(fp, "delay_usec offset_usec\n\n");
    fclose(fp);

    // Send request bursts to NTP server
    for(int i = 0; i < 15; i++) {
        send_request_burst(i);
        printf("\n");
    }
    
    return 0;
}