/* The attack demo without cFS.
 *
 * This program plays the mission-critical process: it starts the bus and the
 * payload in their containers, sends the payload one ATTACK command after
 * another, watches heartbeats and restart requests, and restarts the payload
 * when it has to. The bus and the payload are the ones the cFS demo uses.
 *
 * Run with four processors (ATMO_QEMU_SMP=4).
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "atmo_rt.h"
#include "demo_mc.h"
#include "demo_msg.h"
#include "demo_ring.h"

#define MS 1000000ull

static int failures, checks;
#define CHECK(cond)                                                        \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            failures++;                                                    \
            printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

void demo_mc_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

/* Something of this process for the payload to try to read. */
static volatile uint64_t secret = 0x5ec4e7;

/* What arrived from the bus. */
static uint32_t imager_hk, reports, bus_events[8], bus_hk, restart_requests, restarts;
static uint32_t pointings, last_instance;
static int32_t  max_abs;
static struct demo_bus_hk last_bus_hk;
static uint16_t seq;

static void restart_payload(const char *why)
{
    uint64_t t0 = art_clock_ns();
    printf("SUPERVISOR: restarting PAYLOAD: %s\n", why);
    demo_mc_stop(DEMO_CHILD_PAYLOAD);
    demo_mc_start(DEMO_CHILD_PAYLOAD, "/cf/payload.elf");
    restarts++;
    printf("SUPERVISOR: PAYLOAD restarted in %lu ms\n", (unsigned long)((art_clock_ns() - t0) / MS));
}

static void handle(const uint8_t *pkt, int len)
{
    uint16_t mid = demo_hdr_mid(pkt);
    switch (mid) {
    case DEMO_IMAGER_HK_TLM_MID: {
        struct demo_imager_hk hk;
        memcpy(&hk, pkt + DEMO_TLM_HDR, sizeof(hk));
        imager_hk++;
        last_instance = hk.generation;
        break;
    }
    case DEMO_PAYLOAD_REPORT_MID:
        reports++;
        printf("MC: report from payload: %.96s\n", (const char *)pkt + DEMO_TLM_HDR);
        break;
    case DEMO_BUS_EVENT_TLM_MID: {
        struct demo_bus_event ev;
        memcpy(&ev, pkt + DEMO_TLM_HDR, sizeof(ev));
        if (ev.reason < 8)
            bus_events[ev.reason]++;
        break;
    }
    case DEMO_BUS_HK_TLM_MID:
        memcpy(&last_bus_hk, pkt + DEMO_TLM_HDR, sizeof(last_bus_hk));
        bus_hk++;
        break;
    case DEMO_ADCS_CMD_MID: {
        struct demo_pointing_cmd p;
        memcpy(&p, pkt + DEMO_CMD_HDR, sizeof(p));
        pointings++;
        for (int i = 0; i < 3; i++) {
            int32_t v = (&p.roll)[i];
            if (v < 0)
                v = -v;
            if (v > max_abs)
                max_abs = v;
        }
        break;
    }
    case DEMO_SUPERVISOR_CMD_MID: {
        struct demo_restart_request rq;
        memcpy(&rq, pkt + DEMO_CMD_HDR, sizeof(rq));
        restart_requests++;
        if (rq.process == DEMO_PROC_PAYLOAD)
            restart_payload("the bus asked for it");
        break;
    }
    default:
        printf("MC: unexpected message 0x%04X, %d bytes\n", mid, len);
    }
}

/* Runs MC's loop for `ns`: messages, heartbeats, restarts. */
static void run_for(uint64_t ns)
{
    static uint64_t seen_beat, seen_at;
    uint64_t        until = art_clock_ns() + ns;
    uint8_t         rec[DEMO_RING_MAX_REC];
    int             len;

    while (art_clock_ns() < until) {
        uint64_t now = art_clock_ns(), beat = demo_mc_heartbeat(DEMO_CHILD_PAYLOAD);
        demo_mc_beat();
        while ((len = demo_mc_recv(rec)) > 0)
            handle(rec, len);
        if (beat != seen_beat || seen_at == 0) {
            seen_beat = beat;
            seen_at   = now;
        } else if (now - seen_at > 500 * MS) {
            printf("SUPERVISOR: PAYLOAD heartbeat stalled for %lu ms\n", (unsigned long)((now - seen_at) / MS));
            restart_payload("its heartbeat stalled");
            seen_at = 0;
        }
        art_task_sleep(5 * MS);
    }
}

static void attack(uint32_t n, uint64_t arg)
{
    uint8_t                pkt[DEMO_CMD_HDR + sizeof(struct demo_attack_cmd)] = {0};
    struct demo_attack_cmd cmd = {n, 0, arg};
    demo_hdr_init(pkt, DEMO_PAYLOAD_CMD_MID, sizeof(pkt), seq++);
    pkt[6] = DEMO_PAYLOAD_ATTACK_CC;
    memcpy(pkt + DEMO_CMD_HDR, &cmd, sizeof(cmd));
    printf("MC: sending ATTACK(%u)\n", (unsigned)n);
    CHECK(demo_mc_send(pkt, sizeof(pkt)) == 0);
}

int main(void)
{
    size_t      size;
    const void *tar = art_boot_payload(&size);
    uint32_t    r0, e0, p0, h0;

    printf("attack demo without cFS\n");
    CHECK(art_fs_add_tar(tar, size) >= 2);
    CHECK(demo_mc_init() == 0);
    CHECK(demo_mc_start(DEMO_CHILD_BUS, "/cf/bus.elf") == 0);
    CHECK(demo_mc_start(DEMO_CHILD_PAYLOAD, "/cf/payload.elf") == 0);

    printf("== normal traffic\n");
    run_for(3500 * MS);
    CHECK(imager_hk >= 3);
    CHECK(pointings >= 1);
    CHECK(restarts == 0);

    printf("== B3: a pointing request out of range\n");
    p0 = pointings, e0 = bus_events[DEMO_DENY_RANGE];
    attack(DEMO_ATTACK_RANGE, 0);
    run_for(1500 * MS);
    CHECK(pointings - p0 >= 2);
    CHECK(bus_events[DEMO_DENY_RANGE] - e0 == 1);
    CHECK(max_abs <= DEMO_ADCS_MAX_MILLIDEG);

    printf("== B1: commands the payload may not send\n");
    e0 = bus_events[DEMO_DENY_SENDER] + bus_events[DEMO_DENY_COMMAND];
    attack(DEMO_ATTACK_FORBIDDEN, 0);
    run_for(1500 * MS);
    CHECK(bus_events[DEMO_DENY_SENDER] + bus_events[DEMO_DENY_COMMAND] - e0 == 2);

    printf("== B2: telemetry under another sender's ID\n");
    e0 = bus_events[DEMO_DENY_SENDER];
    attack(DEMO_ATTACK_SPOOF, 0);
    run_for(1500 * MS);
    CHECK(bus_events[DEMO_DENY_SENDER] - e0 == 1);
    CHECK(restarts == 0);

    printf("== K2: around the bus\n");
    r0 = reports;
    attack(DEMO_ATTACK_BYPASS, 0);
    run_for(1500 * MS);
    CHECK(reports - r0 == 5);

    printf("== K3: all the memory it can get\n");
    r0 = reports, h0 = imager_hk;
    attack(DEMO_ATTACK_EXHAUST, 0);
    run_for(2500 * MS);
    CHECK(reports - r0 == 2);
    CHECK(imager_hk - h0 >= 1);
    CHECK(restarts == 0);

    printf("== K1: read MC memory at %p\n", (void *)&secret);
    r0 = restarts;
    attack(DEMO_ATTACK_READ_MC, (uint64_t)(uintptr_t)&secret);
    run_for(3000 * MS);
    CHECK(restarts - r0 == 1);
    CHECK(secret == 0x5ec4e7);

    printf("== K1: write MC's ring\n");
    r0 = restarts;
    attack(DEMO_ATTACK_WRITE_RING, 0);
    run_for(3000 * MS);
    CHECK(restarts - r0 == 1);

    printf("== B4: malformed packets\n");
    r0 = restarts, e0 = bus_events[DEMO_DENY_MALFORMED], h0 = bus_hk;
    attack(DEMO_ATTACK_MALFORMED, 0);
    run_for(3000 * MS);
    CHECK(bus_events[DEMO_DENY_MALFORMED] - e0 == 4);
    CHECK(bus_hk - h0 >= 2); /* the bus still runs */
    CHECK(restarts - r0 == 0);

    printf("== B5: flood\n");
    r0 = restarts, e0 = bus_events[DEMO_DENY_RATE];
    attack(DEMO_ATTACK_FLOOD, 0);
    run_for(4000 * MS);
    CHECK(bus_events[DEMO_DENY_RATE] - e0 >= 1);
    CHECK(restarts - r0 == 1);

    printf("== B6: change after check\n");
    r0 = restarts;
    attack(DEMO_ATTACK_TOCTOU, 0);
    run_for(4000 * MS);
    CHECK(max_abs <= DEMO_ADCS_MAX_MILLIDEG);
    /* Whether the bus happens to read a forbidden value, and so whether it
     * asks for a restart, depends on timing. */
    printf("  restarts during B6: %u, range denials so far: %u\n", (unsigned)(restarts - r0),
           (unsigned)bus_events[DEMO_DENY_RANGE]);

    printf("== after the attacks\n");
    h0 = imager_hk;
    run_for(2500 * MS);
    CHECK(imager_hk - h0 >= 2);
    CHECK(last_instance == demo_mc_payload_instance());
    printf("payload instances %u, restarts %u, restart requests %u, pointings accepted %u, "
           "largest angle %d\n",
           (unsigned)demo_mc_payload_instance(), (unsigned)restarts, (unsigned)restart_requests,
           (unsigned)pointings, (int)max_abs);
    printf("bus: forwarded mc=%u payload=%u, denied payload=%u, dropped payload=%u\n",
           (unsigned)last_bus_hk.forwarded[0], (unsigned)last_bus_hk.forwarded[2],
           (unsigned)last_bus_hk.denied[2], (unsigned)last_bus_hk.dropped[2]);
    demo_mc_stop(DEMO_CHILD_PAYLOAD);
    demo_mc_stop(DEMO_CHILD_BUS);

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
