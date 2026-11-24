/*
 * The payload of the attack demo: an imager application that runs alone in
 * its own container, and that can be told to misbehave.
 *
 * It maps its image, its stack, and two rings to the bus. Normally it sends
 * housekeeping once a second and asks attitude control for a pointing every
 * five seconds. The command ATTACK(n) from the mission-critical process makes
 * it act as if it were compromised; see DEMO_ATTACK_ in demo_msg.h.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "demo_layout.h"
#include "demo_msg.h"
#include "demo_ring.h"
#include "guest.h"

#define MS 1000000ull

/* What the cFS bundle uses for the executive and for SAMPLE_APP. */
#define CFE_ES_CMD_MID        0x1806
#define CFE_ES_RESTART_APP_CC 6
#define SAMPLE_APP_HK_TLM_MID 0x0883

static struct demo_ring_end out, in;
static uint16_t             seq;
static uint32_t             hk_counter, attacks_seen, generation;
static uint8_t              cmd_count, err_count;

static int send(const uint8_t *pkt, uint32_t len)
{
    return demo_ring_put(&out, pkt, len);
}

static int send_tlm(uint16_t mid, const void *payload, uint32_t size)
{
    uint8_t pkt[DEMO_TLM_HDR + 128] = {0};
    demo_hdr_init(pkt, mid, (uint16_t)(DEMO_TLM_HDR + size), seq++);
    memcpy(pkt + DEMO_TLM_HDR, payload, size);
    return send(pkt, DEMO_TLM_HDR + size);
}

static int send_cmd(uint16_t mid, uint8_t cc, const void *payload, uint32_t size)
{
    uint8_t pkt[DEMO_CMD_HDR + 128] = {0};
    uint8_t sum = 0xff;
    demo_hdr_init(pkt, mid, (uint16_t)(DEMO_CMD_HDR + size), seq++);
    pkt[6] = cc;
    if (size != 0)
        memcpy(pkt + DEMO_CMD_HDR, payload, size);
    for (uint32_t i = 0; i < DEMO_CMD_HDR + size; i++)
        sum ^= pkt[i];
    pkt[7] = sum;
    return send(pkt, DEMO_CMD_HDR + size);
}

/* Prints a line on the console and sends it to MC through the bus. */
static void report(const char *fmt, ...)
{
    char    text[DEMO_REPORT_TEXT] = {0};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    printf("PAYLOAD: %s\n", text);
    send_tlm(DEMO_PAYLOAD_REPORT_MID, text, sizeof(text));
}

static void send_pointing(int32_t roll, int32_t pitch, int32_t yaw)
{
    struct demo_pointing_cmd p = {roll, pitch, yaw};
    send_cmd(DEMO_ADCS_CMD_MID, DEMO_ADCS_SET_POINTING_CC, &p, sizeof(p));
}

static void beat_for(uint64_t ns)
{
    uint64_t until = guest_clock_ns() + ns;
    while (guest_clock_ns() < until) {
        out.ring->heartbeat++;
        guest_spin_ns(1 * MS);
    }
}

/* K2: every way around the bus that the kernel offers. */
static void attack_bypass(void)
{
    long first = 0, same = 1, r;
    for (int d = 0; d < 32; d++) {
        r = guest_sys1(NR_SEND_EMPTY_NO_BLOCK, d);
        if (d == 0)
            first = r;
        else if (r != first)
            same = 0;
    }
    report("K2 send on descriptors 0..31: %ld%s", first, same ? " for each" : " (results differ)");
    report("K2 send_pages on descriptor 0: %ld", guest_sys3(NR_SEND_PAGES, 0, out.ring, 1));
    report("K2 send_endpoint on descriptor 0: %ld", guest_sys2(NR_SEND_ENDPOINT, 0, 0));
    report("K2 new_container: %ld",
           guest_syscall(NR_NEW_CONTAINER, (long)out.ring, 1, 0, 0, DEMO_PL_CPU, (long)guest_main, 0));
    report("K2 kill_container(0): %ld  kill_process(0): %ld", guest_sys1(NR_KILL_CONTAINER, 0),
           guest_sys1(NR_KILL_PROCESS, 0));
}

/* K3: take all the memory the container may have. */
static void attack_exhaust(void)
{
    uintptr_t va    = 0xc000000000ull;
    long      pages = 0, r, q;
    /* Move the container's quota to the process and the thread, a page at a
     * time, until there is none. */
    while ((q = guest_sys1(NR_ALLOC_QUOTA_4K, 1)) >= 0)
        ;
    while (guest_sys1(NR_ALLOC_THREAD_QUOTA_4K, 1) >= 0)
        ;
    while ((r = guest_sys2(NR_MMAP_4K, va + (uintptr_t)pages * 4096, 1)) >= 0 && pages < 100000) {
        *(volatile char *)(va + (uintptr_t)pages * 4096) = 1;
        pages++;
    }
    report("K3 mapped %ld pages, then mmap_4k: %ld (quota: %ld)", pages, r, q);
    /* Hold the processor without a kernel call for a while. */
    guest_spin_ns(300 * MS);
    report("K3 spun 300 ms");
}

/* B6: publish an allowed pointing request, then keep rewriting its angle
 * between the allowed value and a forbidden one while the bus reads it. */
static void attack_toctou(void)
{
    struct demo_ring *r = out.ring;
    uint32_t          rounds;
    for (rounds = 0; rounds < 60; rounds++) {
        uint32_t at   = out.mine + 2 + DEMO_CMD_HDR; /* the roll field of the next record */
        uint64_t stop = guest_rdtsc() + guest_tsc_hz() / 500;
        int32_t  bad = 90000, good = 5000;
        send_pointing(good, good, good);
        while (guest_rdtsc() < stop) {
            demo_ring_copy_in(r, at, (const uint8_t *)&bad, 4);
            for (volatile int i = 0; i < 50; i++)
                ;
            demo_ring_copy_in(r, at, (const uint8_t *)&good, 4);
            for (volatile int i = 0; i < 50; i++)
                ;
        }
        r->heartbeat++;
        /* Stay under the rate limit, so that the bus reads every record. */
        beat_for(8 * MS);
    }
    report("B6 rewrote %u pointing records after publishing them", (unsigned)rounds);
}

static void attack(uint32_t n, uint64_t arg)
{
    attacks_seen++;
    printf("PAYLOAD: ATTACK %u begins, tsc %lu\n", (unsigned)n, (unsigned long)guest_rdtsc());
    fflush(stdout);
    switch (n) {
    case DEMO_ATTACK_READ_MC:
        report("K1 reading MC memory at %#lx", (unsigned long)arg);
        beat_for(20 * MS); /* let the report out first */
        printf("PAYLOAD: K1 read begins, tsc %lu\n", (unsigned long)guest_rdtsc());
        fflush(stdout);
        report("K1 READ RETURNED %#lx", (unsigned long)*(volatile uint64_t *)(uintptr_t)arg);
        break;
    case DEMO_ATTACK_WRITE_RING:
        report("K1 writing MC's ring at %#lx", (unsigned long)DEMO_ADDR(DEMO_MC_OUT_PAGE));
        beat_for(20 * MS);
        printf("PAYLOAD: K1 write begins, tsc %lu\n", (unsigned long)guest_rdtsc());
        fflush(stdout);
        *(volatile uint32_t *)DEMO_ADDR(DEMO_MC_OUT_PAGE) = 0xdeadbeef;
        report("K1 WRITE RETURNED");
        break;
    case DEMO_ATTACK_BYPASS:
        attack_bypass();
        break;
    case DEMO_ATTACK_EXHAUST:
        attack_exhaust();
        break;
    case DEMO_ATTACK_FORBIDDEN: {
        char app[20] = "SAMPLE_APP";
        send_cmd(CFE_ES_CMD_MID, CFE_ES_RESTART_APP_CC, app, sizeof(app));
        send_cmd(DEMO_ADCS_CMD_MID, DEMO_ADCS_RESET_CC, NULL, 0);
        report("B1 sent ES restart-app for SAMPLE_APP and ADCS reset");
        break;
    }
    case DEMO_ATTACK_SPOOF: {
        uint8_t hk[12] = {77, 0};
        send_tlm(SAMPLE_APP_HK_TLM_MID, hk, sizeof(hk));
        report("B2 sent telemetry as SAMPLE_APP_HK");
        break;
    }
    case DEMO_ATTACK_RANGE:
        send_pointing(1000, 2000, 3000);
        send_pointing(90000, 0, 0);
        send_pointing(4000, 5000, 6000);
        report("B3 sent pointing 1000,2000,3000 then 90000,0,0 then 4000,5000,6000");
        break;
    case DEMO_ATTACK_MALFORMED: {
        uint8_t pkt[DEMO_CMD_HDR + 12] = {0};
        /* A length field larger than the record. */
        demo_hdr_init(pkt, DEMO_ADCS_CMD_MID, 500, seq++);
        pkt[6] = DEMO_ADCS_SET_POINTING_CC;
        send(pkt, sizeof(pkt));
        /* A command cut off inside its secondary header. */
        demo_hdr_init(pkt, DEMO_ADCS_CMD_MID, 7, seq++);
        send(pkt, 7);
        /* Stay under the bus's threshold of denials per second, so that it
         * reads the other two kinds as well. */
        beat_for(1200 * MS);
        /* Three bytes. */
        send(pkt, 3);
        beat_for(20 * MS);
        /* A record whose length the ring cannot hold. */
        out.ring->data[out.mine & DEMO_RING_MASK]       = 0xff;
        out.ring->data[(out.mine + 1) & DEMO_RING_MASK] = 0xff;
        out.mine += 64;
        out.ring->tail = out.mine;
        beat_for(20 * MS);
        report("B4 sent four malformed records");
        break;
    }
    case DEMO_ATTACK_FLOOD: {
        struct demo_imager_hk hk = {0};
        uint64_t              until = guest_clock_ns() + 2000 * MS;
        uint32_t              sent  = 0;
        while (guest_clock_ns() < until) {
            hk.counter = sent;
            if (send_tlm(DEMO_IMAGER_HK_TLM_MID, &hk, sizeof(hk)) == 0)
                sent++;
            out.ring->heartbeat++;
        }
        report("B5 flooded for 2 s, %u packets", (unsigned)sent);
        break;
    }
    case DEMO_ATTACK_TOCTOU:
        attack_toctou();
        break;
    default:
        err_count++;
        report("unknown attack %u", (unsigned)n);
    }
}

static void poll_commands(void)
{
    uint8_t rec[DEMO_RING_MAX_REC];
    int     len;
    while ((len = demo_ring_get(&in, rec)) > 0) {
        if (len < DEMO_CMD_HDR || demo_hdr_mid(rec) != DEMO_PAYLOAD_CMD_MID) {
            err_count++;
            continue;
        }
        cmd_count++;
        if ((rec[6] & 0x7f) == DEMO_PAYLOAD_ATTACK_CC && len == DEMO_CMD_HDR + (int)sizeof(struct demo_attack_cmd)) {
            struct demo_attack_cmd cmd;
            memcpy(&cmd, rec + DEMO_CMD_HDR, sizeof(cmd));
            attack(cmd.attack, cmd.arg);
        }
    }
}

void guest_main(uintptr_t range, size_t pages)
{
    uint64_t next_hk, next_pointing;
    uint32_t pointings = 0;

    out.ring   = (struct demo_ring *)range;
    in.ring    = (struct demo_ring *)(range + DEMO_RING_PAGES * DEMO_PAGE);
    generation = (uint32_t)out.ring->arg[0];
    printf("PAYLOAD: started, generation %u, %lu pages at %#lx, tsc %lu\n", (unsigned)generation,
           (unsigned long)pages, (unsigned long)range, (unsigned long)guest_rdtsc());
    out.ring->status = 1;
    next_hk       = guest_clock_ns() + 200 * MS;
    next_pointing = guest_clock_ns() + 1500 * MS;

    for (;;) {
        uint64_t now = guest_clock_ns();
        out.ring->heartbeat++;
        poll_commands();
        if (now >= next_hk) {
            struct demo_imager_hk hk = {cmd_count, err_count, 0, hk_counter++, attacks_seen, generation};
            send_tlm(DEMO_IMAGER_HK_TLM_MID, &hk, sizeof(hk));
            next_hk += 1000 * MS;
        }
        if (now >= next_pointing) {
            int32_t a = 1000 * (int32_t)(1 + pointings % 20);
            send_pointing(a, -a, a / 2);
            pointings++;
            next_pointing += 5000 * MS;
        }
        guest_spin_ns(1 * MS);
    }
}
