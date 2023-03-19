#include <stdint.h>

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