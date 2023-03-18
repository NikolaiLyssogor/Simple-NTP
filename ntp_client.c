#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#define SERVERADDR "utcnist.colorado.edu"
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

typedef struct {
    uint32_t delay_int; // Seconds of delay
    uint32_t delay_frac; // Picoseconds? of delay
    uint32_t offset_int; // Seconds of offset
    uint32_t offset_frac; // Picoseconds? of offset
} update_value;

uint32_t timeval_to_ntp_seconds(time_t seconds) {
    /*
    Converts the seconds value of local Unix-epoch 
    timestamps to NTP-epoch fractions.
    https://tickelton.gitlab.io/articles/ntp-timestamps/
    */
   return seconds + NTP_EPOCH_OFFSET;
}

uint32_t timeval_to_ntp_frac(long frac) {
    /*
    Converts the fractional second value of local
    Unix-epoch timestamps to NTP-epoch fractions. 
    https://tickelton.gitlab.io/articles/ntp-timestamps/
    */
   return (uint32_t)((double)(frac + 1) * (double)(1LL << 32) * 1.0e-6);
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
    values->delay_int = (dest_ts_int - packet.org_ts_int) - (packet.trans_ts_int - packet.recv_ts_int);
    values->delay_frac = (dest_ts_frac - packet.org_ts_frac) - (packet.trans_ts_frac - packet.recv_ts_frac);
    values->offset_int = 0.5*((packet.recv_ts_int - packet.org_ts_int) + (packet.trans_ts_int - dest_ts_int));
    values->offset_frac = 0.5*((packet.recv_ts_frac - packet.org_ts_frac) + (packet.trans_ts_frac - dest_ts_frac));

    // TODO: Convert fraction to base-10 picoseconds?

    // printf("delay seconds: %u\n", values->delay_int);
    // printf("delay fraction: %u ", values->delay_frac);
    // print_uint32_t_binary(values->delay_frac);
    // printf("offset seconds: %u\n", values->offset_int);
    // printf("offset fraction: %u ", values->offset_frac);
    // print_uint32_t_binary(values->offset_frac);
}

update_value send_request() {
    /*
    Makes a request to an NTP server and returns the delay and offset 
    for that request. Uses a new socket for each request. Retransmits
    the packet if the bytes sent or the bytes received is not what is
    expected. Returns error value for all other errors.
    */
   
    // Update value to return in case of error
    update_value error_values = { 0.0, 0.0, 0.0, 0.0 };
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
    fp = fopen("nist_results.txt", "a");

    // Send burst of 8 requests
    for(int i = 0; i < 8; i++) {
        printf("Sending request %d in burst %d\n", i+1, burst_num+1);

        // Make NTP request
        update_value vals = send_request();

        // Write results to text file
        fprintf(fp, "%u             %u       %u              %u\n",
        vals.delay_int, vals.delay_frac, vals.offset_int, vals.offset_frac);
        sleep(4);
    }
    fprintf(fp, "\n");
    fclose(fp);
}

int main() {
    // Clear the contents of the results file and write the header
    FILE *fp;
    fp = fopen("nist_results.txt", "w");
    fprintf(fp, "delay_seconds delay_fraction offset_seconds offset_fraction\n");
    fclose(fp);

    // Send request bursts to NTP server
    for(int i = 0; i < 2; i++) {
        send_request_burst(i);
        printf("\n");
    }
    
    return 0;
}