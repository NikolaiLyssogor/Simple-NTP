#include <stdint.h>

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