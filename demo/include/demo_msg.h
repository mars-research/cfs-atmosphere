/*
 * The messages of the demo, for every component: the cFS applications in the
 * mission-critical process (MC), the bus, and the payload.
 *
 * Packets are CCSDS space packets as cFE builds them with its default
 * headers: a primary header of 6 bytes, then a command secondary header of
 * 2 bytes or a telemetry secondary header of 10 bytes. Multi-byte header
 * fields are big-endian; the fields of a payload are little-endian.
 *
 * Message IDs follow the cFS bundle: 0x1800 | topic for a command and
 * 0x0800 | topic for telemetry. Topics 0xD0 to 0xD7 are unused there.
 * The bus (demo/bus/src/msg.rs, policy.rs) repeats the numbers it needs.
 */
#ifndef DEMO_MSG_H
#define DEMO_MSG_H

#include <stdint.h>

#define DEMO_CMD_HDR 8
#define DEMO_TLM_HDR 16

/* Who sends: the ring a record arrives on, never a field of the packet. */
#define DEMO_PROC_MC      0
#define DEMO_PROC_BUS     1
#define DEMO_PROC_PAYLOAD 2

/* Commands. */
#define DEMO_PAYLOAD_CMD_MID    0x18D0 /* MC -> payload */
#define DEMO_ADCS_CMD_MID       0x18D1 /* payload or MC -> ADCS_SIM */
#define DEMO_SUPERVISOR_CMD_MID 0x18D2 /* bus -> SUPERVISOR */

/* Telemetry. */
#define DEMO_IMAGER_HK_TLM_MID  0x08D0 /* payload -> MC */
#define DEMO_PAYLOAD_REPORT_MID 0x08D1 /* payload -> MC: one line of text */
#define DEMO_BUS_EVENT_TLM_MID  0x08D2 /* bus -> MC: a denied record */
#define DEMO_ADCS_HK_TLM_MID    0x08D3 /* ADCS_SIM, inside MC */
#define DEMO_BUS_HK_TLM_MID     0x08D4 /* bus -> MC: counters */

/* Function codes of DEMO_PAYLOAD_CMD_MID. */
#define DEMO_PAYLOAD_NOOP_CC   0
#define DEMO_PAYLOAD_ATTACK_CC 1
/* Function codes of DEMO_ADCS_CMD_MID. */
#define DEMO_ADCS_NOOP_CC         0
#define DEMO_ADCS_SET_POINTING_CC 1
#define DEMO_ADCS_RESET_CC        2
/* Function codes of DEMO_SUPERVISOR_CMD_MID. */
#define DEMO_SUPERVISOR_RESTART_REQUEST_CC 1

/* The policy lets the payload ask for at most this much on each axis. */
#define DEMO_ADCS_MAX_MILLIDEG 30000

/* The attacks that DEMO_PAYLOAD_ATTACK_CC selects. */
#define DEMO_ATTACK_READ_MC      1  /* K1: read MC memory at `arg` */
#define DEMO_ATTACK_WRITE_RING   2  /* K1: write the ring of another process */
#define DEMO_ATTACK_BYPASS       3  /* K2: reach around the bus with kernel calls */
#define DEMO_ATTACK_EXHAUST      4  /* K3: map pages until refused, then spin */
#define DEMO_ATTACK_FORBIDDEN    5  /* B1: commands the payload may not send */
#define DEMO_ATTACK_SPOOF        6  /* B2: telemetry under another sender's ID */
#define DEMO_ATTACK_RANGE        7  /* B3: a pointing request out of range */
#define DEMO_ATTACK_MALFORMED    8  /* B4: malformed packets */
#define DEMO_ATTACK_FLOOD        9  /* B5: as many packets as it can for 2 s */
#define DEMO_ATTACK_TOCTOU       10 /* B6: change a record after it is checked */

/* Reasons of a denial, in DEMO_BUS_EVENT_TLM_MID and in the bus's log. */
#define DEMO_DENY_MALFORMED 1
#define DEMO_DENY_SENDER    2 /* this sender may not send this message ID */
#define DEMO_DENY_COMMAND   3 /* nor this function code */
#define DEMO_DENY_RANGE     4 /* a field is outside its limits */
#define DEMO_DENY_RATE      5
#define DEMO_DENY_NOROUTE   6

/* Payloads. Each follows its header directly. */
struct demo_attack_cmd {
    uint32_t attack;
    uint32_t pad;
    uint64_t arg;
};
struct demo_pointing_cmd {
    int32_t roll, pitch, yaw; /* millidegrees */
};
struct demo_restart_request {
    uint32_t process; /* DEMO_PROC_ */
    uint32_t reason;  /* DEMO_DENY_ */
    uint32_t denials;
    uint32_t dropped;
};
struct demo_imager_hk {
    uint8_t  cmd_count, err_count; /* as every housekeeping packet starts */
    uint16_t pad;
    uint32_t counter;
    uint32_t attacks_seen;
    uint32_t generation;
};
struct demo_bus_event {
    uint32_t sender;
    uint32_t reason;
    uint32_t msg_id;
    uint32_t fcn_code;
    uint32_t denials; /* of this sender since it started */
};
struct demo_bus_hk {
    uint8_t  cmd_count, err_count;
    uint16_t pad;
    uint32_t forwarded[3]; /* by sender */
    uint32_t denied[3];
    uint32_t dropped[3]; /* over the rate, or the destination was full */
    uint32_t restart_requests;
};
struct demo_adcs_hk {
    uint8_t  cmd_count, err_count;
    uint16_t pad;
    int32_t  roll, pitch, yaw; /* the last accepted pointing */
    uint32_t accepted;
    int32_t  max_abs; /* the largest magnitude ever accepted */
};
#define DEMO_REPORT_TEXT 96

/* Header helpers for code outside cFE. */
static inline void demo_hdr_init(uint8_t *pkt, uint16_t mid, uint16_t total_len, uint16_t seq)
{
    uint16_t len = (uint16_t)(total_len - 7);
    pkt[0] = (uint8_t)(mid >> 8);
    pkt[1] = (uint8_t)mid;
    pkt[2] = (uint8_t)(0xC0 | ((seq >> 8) & 0x3F));
    pkt[3] = (uint8_t)seq;
    pkt[4] = (uint8_t)(len >> 8);
    pkt[5] = (uint8_t)len;
}
static inline uint16_t demo_hdr_mid(const uint8_t *pkt)
{
    return (uint16_t)((pkt[0] << 8) | pkt[1]);
}
static inline uint16_t demo_hdr_len(const uint8_t *pkt)
{
    return (uint16_t)(((pkt[4] << 8) | pkt[5]) + 7);
}

#endif
