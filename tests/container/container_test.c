/* Test of child containers: create, run, isolate, detect, kill, restart.
 *
 * The first process loads a second image from the boot payload into fresh
 * pages, creates a container that maps those pages, and gives it a
 * processor. The child increments a heartbeat in a shared page. The test
 * checks that
 *
 *   - the child runs on its own processor while the parent keeps running;
 *   - the child keeps running after the parent unmaps the child's image and
 *     stack from its own address space;
 *   - a read of the parent's memory faults in the child and ends only the
 *     child's thread;
 *   - the parent notices the stalled heartbeat, kills the container, and
 *     starts a new one in its place.
 *
 * Run with at least two processors (ATMO_QEMU_SMP=2).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "atmo_rt.h"
#include "demo_layout.h"
#include "demo_ring.h"
#include "elf_load.h"

#define MS 1000000ull

static int failures;
static int checks;

#define CHECK(cond)                                                        \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            failures++;                                                    \
            printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

static const char *name_of(int64_t code)
{
    static char buf[48];
    art_sys_error_name(code, buf, sizeof(buf));
    return buf;
}

/* Something of the parent for the child to try to read. */
static volatile uint64_t secret = 0x5ec4e7;

static struct demo_ring *const ring = (struct demo_ring *)DEMO_ADDR(DEMO_PL_OUT_PAGE);
#define PRIVATE_PAGE  DEMO_PL_STACK_PAGE /* stack and image: the child's alone */
#define PRIVATE_PAGES (DEMO_REGION_PAGES - DEMO_PL_STACK_PAGE)

/* Loads the image into fresh pages and starts a container on it. */
static int64_t start_child(const void *image, size_t size, uint64_t victim, uint64_t when)
{
    uintptr_t entry, sp;
    size_t    bytes;
    int64_t   ret;
    int       rc;

    if (art_sys_map(DEMO_ADDR(PRIVATE_PAGE), PRIVATE_PAGES) != 0) {
        printf("  could not map the child's pages\n");
        return -1;
    }
    rc = demo_elf_load(image, size, DEMO_ADDR(DEMO_PL_IMAGE_PAGE), DEMO_PL_IMAGE_PAGES * DEMO_PAGE,
                       &entry, &bytes);
    if (rc != 0) {
        printf("  could not load the image: %d\n", rc);
        return -1;
    }
    memset(ring, 0, 2 * DEMO_RING_PAGES * DEMO_PAGE);
    ring->arg[0] = victim;
    ring->arg[1] = when;
    sp = DEMO_ADDR(DEMO_PL_IMAGE_PAGE) - 8;

    /* The processor may still be on its way off after a kill. */
    for (int tries = 0;; tries++) {
        ret = art_sys_new_container(DEMO_ADDR(DEMO_PL_RANGE_PAGE), DEMO_PL_RANGE_PAGES, 64, 32,
                                    DEMO_PL_CPU, -1, entry, sp);
        if (ret >= 0 || tries == 200)
            break;
        art_task_sleep(5 * MS);
    }
    printf("  image %lu bytes, entry %#lx; new_container: %s\n", (unsigned long)bytes,
           (unsigned long)entry, ret >= 0 ? "ok" : name_of(ret));
    return ret;
}

/* Waits until the heartbeat reaches `count`. Returns 0 on a timeout. */
static int wait_heartbeat(uint64_t count, uint64_t limit_ns)
{
    uint64_t deadline = art_clock_ns() + limit_ns;
    while (ring->heartbeat < count) {
        if (art_clock_ns() > deadline)
            return 0;
        art_task_sleep(1 * MS);
    }
    return 1;
}

/* Waits until the heartbeat has not moved for `stall_ns`. Returns the time
 * at which it was last seen to move, or 0 on a timeout. */
static uint64_t wait_stall(uint64_t stall_ns, uint64_t limit_ns)
{
    uint64_t deadline = art_clock_ns() + limit_ns;
    uint64_t seen = ring->heartbeat, since = art_clock_ns();
    for (;;) {
        uint64_t now = art_clock_ns();
        if (ring->heartbeat != seen) {
            seen  = ring->heartbeat;
            since = now;
        } else if (now - since >= stall_ns) {
            return since;
        }
        if (now > deadline)
            return 0;
        art_task_sleep(1 * MS);
    }
}

int main(void)
{
    size_t      size;
    const void *image = art_boot_payload(&size);
    int64_t     ret;
    uint64_t    t0, t1, last;

    printf("container test\n");
    CHECK(size != 0);
    if (size == 0)
        return 1;
    /* The rings stay mapped in the parent for the whole test. */
    CHECK(art_sys_map(DEMO_ADDR(DEMO_PL_OUT_PAGE), 2 * DEMO_RING_PAGES) == 0);

    printf("start: a child that reads the parent's memory at heartbeat 60\n");
    t0  = art_clock_ns();
    ret = start_child(image, size, (uint64_t)(uintptr_t)&secret, 60);
    CHECK(ret >= 0);
    ret = art_sys_cpu_online_child(0, DEMO_PL_CPU);
    CHECK(ret >= 0);
    CHECK(wait_heartbeat(5, 5000 * MS));
    t1 = art_clock_ns();
    printf("  first heartbeats %lu ms after the start\n", (unsigned long)((t1 - t0) / MS));

    printf("unmap: the parent drops its mapping of the child's stack and image\n");
    ret = art_sys_unmap(DEMO_ADDR(PRIVATE_PAGE), PRIVATE_PAGES);
    printf("  unmap: %s\n", ret >= 0 ? "ok" : name_of(ret));
    CHECK(ret >= 0);
    last = ring->heartbeat;
    CHECK(wait_heartbeat(last + 10, 2000 * MS));
    printf("  the child still runs: heartbeat %lu\n", (unsigned long)ring->heartbeat);

    printf("attack: wait for the child to stall\n");
    last = wait_stall(500 * MS, 10000 * MS);
    t1   = art_clock_ns();
    CHECK(last != 0);
    printf("  heartbeat stopped at %lu; noticed %lu ms after its last move\n",
           (unsigned long)ring->heartbeat, (unsigned long)((t1 - last) / MS));
    CHECK(ring->heartbeat == 60);
    /* The child sets status to what it read, or to 99 if the read returned. */
    CHECK(ring->status == 1);
    CHECK(secret == 0x5ec4e7);

    printf("respond: kill the container and start a new one\n");
    t0  = art_clock_ns();
    ret = art_sys_kill_container(0);
    t1  = art_clock_ns();
    printf("  kill_container: %s, %lu us\n", ret >= 0 ? "ok" : name_of(ret),
           (unsigned long)((t1 - t0) / 1000));
    CHECK(ret >= 0);
    /* The processor came back idle; a new container needs it off. */
    ret = art_sys_cpu_offline_request(DEMO_PL_CPU);
    printf("  cpu_offline_request: %s\n", ret >= 0 ? "ok" : name_of(ret));
    ret = start_child(image, size, 0, 0);
    CHECK(ret >= 0);
    ret = art_sys_cpu_online_child(0, DEMO_PL_CPU);
    CHECK(ret >= 0);
    CHECK(wait_heartbeat(5, 5000 * MS));
    t1 = art_clock_ns();
    printf("  the new child runs %lu ms after the kill began\n", (unsigned long)((t1 - t0) / MS));
    last = ring->heartbeat;
    CHECK(wait_heartbeat(last + 100, 5000 * MS));
    CHECK(ring->status == 1);

    printf("cleanup: kill the second container\n");
    ret = art_sys_kill_container(0);
    CHECK(ret >= 0);
    ret = art_sys_kill_container(0);
    printf("  a second kill: %s\n", ret >= 0 ? "ok" : name_of(ret));
    CHECK(ret < 0);

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
