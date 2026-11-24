/* See demo_mc.h. */
#include "demo_mc.h"

#include <string.h>

#include "atmo_rt.h"
#include "demo_layout.h"
#include "demo_ring.h"
#include "elf_load.h"

#define MS 1000000ull

struct child {
    const char *name;
    /* The range of MC's pages that the child maps. */
    uint64_t range_page, range_pages;
    /* The part of the range that no other process may see: image and stack. */
    uint64_t private_page, private_pages;
    uint64_t image_page, image_pages;
    /* The stack ends where this page begins. */
    uint64_t stack_end_page;
    unsigned cpu;
    /* Pages of memory quota for the container, all given to its process. */
    unsigned funding;
    /* Whether the child gets MC's endpoint 0, to receive pages on. */
    int      endpoint;
    int      running;
    int      probed;
};

static struct child children[DEMO_CHILDREN] = {
    [DEMO_CHILD_BUS] = {
        .name = "BUS",
        .range_page = DEMO_BUS_RANGE_PAGE, .range_pages = DEMO_BUS_RANGE_PAGES,
        .private_page = DEMO_BUS_IMAGE_PAGE, .private_pages = DEMO_BUS_IMAGE_PAGES + DEMO_STACK_PAGES,
        .image_page = DEMO_BUS_IMAGE_PAGE, .image_pages = DEMO_BUS_IMAGE_PAGES,
        .stack_end_page = DEMO_BUS_STACK_PAGE + DEMO_STACK_PAGES,
        /* The bus pays for the page tables under the payload's rings. */
        .cpu = DEMO_BUS_CPU, .funding = 64, .endpoint = 1,
    },
    [DEMO_CHILD_PAYLOAD] = {
        .name = "PAYLOAD",
        .range_page = DEMO_PL_RANGE_PAGE, .range_pages = DEMO_PL_RANGE_PAGES,
        /* All of it: MC keeps no page of the payload, its rings included. */
        .private_page = DEMO_PL_RANGE_PAGE, .private_pages = DEMO_PL_RANGE_PAGES,
        .image_page = DEMO_PL_IMAGE_PAGE, .image_pages = DEMO_PL_IMAGE_PAGES,
        .stack_end_page = DEMO_PL_STACK_PAGE + DEMO_STACK_PAGES,
        .cpu = DEMO_PL_CPU, .funding = 16,
    },
};

/* The kernel numbers the child containers in the order of their creation. */
static int order[DEMO_CHILDREN];
static int order_len;

static struct demo_ring_end mc_out, mc_in;
static uint32_t             payload_instance;

/* Words in the headers of MC's two rings, which only MC and the bus map.
 * MC writes those of the outbound ring, the bus those of the inbound one. */
#define OUT_GENERATION 0 /* payload generation; odd while there is no payload to serve */
#define OUT_REMAP      1 /* a count: MC is about to send the rings of a new payload */
#define IN_PL_BEAT     1 /* the bus's copy of the payload's heartbeat */
#define IN_REMAP_ACK   2 /* the remap count that the bus is about to receive pages for */
#define IN_REMAP_DONE  3 /* the remap count whose pages the bus has mapped */

/* The largest image. */
static uint8_t image_buf[512 * 1024];

static struct demo_ring *ring_at(uint64_t page)
{
    return (struct demo_ring *)DEMO_ADDR(page);
}

static const char *result(int64_t code)
{
    static char buf[48];
    if (code >= 0)
        return "ok";
    art_sys_error_name(code, buf, sizeof(buf));
    return buf;
}

const char *demo_mc_name(int which)
{
    return children[which].name;
}

int demo_mc_running(int which)
{
    return children[which].running;
}

uint64_t demo_mc_heartbeat(int which)
{
    /* The bus beats in the ring it produces. MC does not map the payload's
     * rings; the bus, which does, passes the payload's heartbeat on. */
    return which == DEMO_CHILD_BUS ? mc_in.ring->heartbeat : mc_in.ring->arg[IN_PL_BEAT];
}

uint32_t demo_mc_payload_instance(void)
{
    return payload_instance;
}

uintptr_t demo_mc_private_base(int which)
{
    return DEMO_ADDR(children[which].private_page);
}

size_t demo_mc_private_pages(int which)
{
    return children[which].private_pages;
}

int demo_mc_init(void)
{
    if (art_sys_map(DEMO_ADDR(DEMO_MC_OUT_PAGE), 2 * DEMO_RING_PAGES) != 0)
        return -1;
    mc_out.ring = ring_at(DEMO_MC_OUT_PAGE);
    mc_in.ring  = ring_at(DEMO_MC_IN_PAGE);
    /* Odd: no payload yet. */
    mc_out.ring->arg[OUT_GENERATION] = 1;
    return 0;
}

/* ---- The probe: can MC still read a page that it gave away? ---- */

static volatile uintptr_t probe_addr;
static volatile int       probe_state; /* 1: about to read; 2: the read returned */
static uint8_t            probe_stack[8192] __attribute__((aligned(16)));

/* A second kernel thread of MC. It touches two words and nothing else. */
static void probe_entry(void)
{
    probe_state = 1;
    (void)*(volatile uint8_t *)probe_addr;
    probe_state = 2;
    art_sys_thread_exit();
}

/* Reads one byte of each page in a separate kernel thread, so that a fault
 * ends that thread and not the cFS thread. Returns the number of pages that
 * MC can still read. */
static int probe(uint64_t page, uint64_t pages)
{
    int readable = 0;
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t deadline;
        probe_addr  = DEMO_ADDR(page + i);
        probe_state = 0;
        if (art_sys_new_thread((uintptr_t)probe_entry, (uintptr_t)probe_stack + sizeof(probe_stack) - 8) < 0)
            return -1;
        /* The thread runs when this one yields. Its read is the instruction
         * after it sets the state, so a few more yields settle the result. */
        deadline = art_clock_ns() + 100 * MS;
        while (probe_state == 0 && art_clock_ns() < deadline)
            art_sys_yield();
        for (int k = 0; k < 4 && probe_state == 1; k++)
            art_sys_yield();
        if (probe_state != 1)
            readable++;
    }
    return readable;
}

/* Sends the rings of the new payload to the bus. The bus first unmaps the
 * rings of the previous payload and says when it is about to receive; MC's
 * send then meets a waiting receiver and does not block. */
static int hand_rings_to_bus(void)
{
    uint64_t request = ++mc_out.ring->arg[OUT_REMAP];
    uint64_t deadline = art_clock_ns() + 1000 * MS;
    int64_t  ret;

    while (mc_in.ring->arg[IN_REMAP_ACK] != request) {
        if (art_clock_ns() > deadline) {
            demo_mc_log("SUPERVISOR: the bus does not take the rings of the payload\n");
            return -1;
        }
        art_task_sleep(1 * MS);
    }
    ret = art_sys_send_pages(0, DEMO_ADDR(DEMO_PL_OUT_PAGE), 2 * DEMO_RING_PAGES);
    if (ret < 0) {
        demo_mc_log("SUPERVISOR: send_pages to the bus: %s\n", result(ret));
        return -1;
    }
    return 0;
}

int demo_mc_send(const void *pkt, uint32_t len)
{
    return mc_out.ring != NULL ? demo_ring_put(&mc_out, pkt, len) : -1;
}

int demo_mc_recv(void *buf)
{
    return mc_in.ring != NULL ? demo_ring_get(&mc_in, buf) : 0;
}

void demo_mc_beat(void)
{
    if (mc_out.ring != NULL)
        mc_out.ring->heartbeat++;
}

static int64_t read_image(const char *path)
{
    int     fd = art_fs_open(path, ART_FS_READ);
    int64_t size;
    if (fd < 0)
        return fd;
    size = art_fs_read(fd, image_buf, sizeof(image_buf));
    art_fs_close(fd);
    return size;
}

int demo_mc_start(int which, const char *path)
{
    struct child *c = &children[which];
    uint64_t      t0 = art_sys_rdtsc();
    uintptr_t     entry;
    size_t        bytes;
    int64_t       size, ret;
    int           rc, tries, readable = 0, shared_ok = 0;

    if (c->running)
        return -1;
    size = read_image(path);
    if (size <= 0) {
        demo_mc_log("SUPERVISOR: cannot read %s: %ld\n", path, (long)size);
        return -1;
    }

    if (which == DEMO_CHILD_BUS) {
        /* A new bus starts MC's rings from empty. */
        uint64_t generation = mc_out.ring->arg[OUT_GENERATION];
        memset(mc_out.ring, 0, 2 * DEMO_RING_PAGES * DEMO_PAGE);
        mc_out.mine = mc_in.mine = 0;
        mc_out.ring->arg[OUT_GENERATION] = generation | 1;
    } else {
        payload_instance++;
    }

    /* Fresh, zeroed pages for everything that is the child's alone. */
    if (art_sys_map(DEMO_ADDR(c->private_page), c->private_pages) != 0) {
        demo_mc_log("SUPERVISOR: cannot map the pages of %s\n", c->name);
        return -1;
    }
    rc = demo_elf_load(image_buf, (size_t)size, DEMO_ADDR(c->image_page), c->image_pages * DEMO_PAGE, &entry,
                       &bytes);
    if (rc != 0) {
        demo_mc_log("SUPERVISOR: cannot load %s: %d\n", path, rc);
        art_sys_unmap(DEMO_ADDR(c->private_page), c->private_pages);
        return -1;
    }
    if (which == DEMO_CHILD_PAYLOAD)
        ring_at(DEMO_PL_OUT_PAGE)->arg[0] = payload_instance;

    /* After a kill the processor is on its way off; the kernel refuses the
     * container until it is. */
    for (tries = 0;; tries++) {
        ret = art_sys_new_container(DEMO_ADDR(c->range_page), c->range_pages, c->funding, c->funding, c->cpu,
                                    c->endpoint ? 0 : -1, entry, DEMO_ADDR(c->stack_end_page) - 8);
        if (ret >= 0 || tries == 400)
            break;
        art_task_sleep(5 * MS);
    }
    if (ret < 0) {
        demo_mc_log("SUPERVISOR: new_container for %s: %s\n", c->name, result(ret));
        art_sys_unmap(DEMO_ADDR(c->private_page), c->private_pages);
        return -1;
    }
    order[order_len++] = which;
    c->running         = 1;
    ret = art_sys_cpu_online_child((size_t)(order_len - 1), c->cpu);
    if (ret < 0)
        demo_mc_log("SUPERVISOR: cpu_online_child for %s: %s\n", c->name, result(ret));

    if (which == DEMO_CHILD_PAYLOAD)
        shared_ok = hand_rings_to_bus() == 0;

    /* MC gives up every page that is the child's. For the payload that is
     * all of them: from here on MC and the payload share no memory. */
    ret = art_sys_unmap(DEMO_ADDR(c->private_page), c->private_pages);
    if (ret < 0)
        demo_mc_log("SUPERVISOR: unmap of %s: %s\n", c->name, result(ret));

    if (which == DEMO_CHILD_PAYLOAD && shared_ok) {
        /* Even: the bus may serve the new payload. */
        mc_out.ring->arg[OUT_GENERATION] = 2 * (uint64_t)payload_instance;
    }
    demo_mc_log("SUPERVISOR: started %s on cpu %u: image %lu bytes, %lu pages given, %lu kept by MC, "
                "%lu us, tsc %lu\n",
                c->name, c->cpu, (unsigned long)bytes, (unsigned long)c->private_pages,
                (unsigned long)(c->range_pages - c->private_pages),
                (unsigned long)((art_sys_rdtsc() - t0) * 1000000ull / art_clock_counter_hz()),
                (unsigned long)art_sys_rdtsc());

    /* Check the unmap: every page of the first instance, and the rings of
     * later ones. Each page that faults costs a line of the kernel's log. */
    if (!c->probed) {
        readable  = probe(c->private_page, c->private_pages);
        c->probed = 1;
        demo_mc_log("SUPERVISOR: probe of %s: MC can read %d of the %lu pages it gave away\n", c->name, readable,
                    (unsigned long)c->private_pages);
    } else if (which == DEMO_CHILD_PAYLOAD) {
        readable = probe(DEMO_PL_OUT_PAGE, 2 * DEMO_RING_PAGES);
        demo_mc_log("SUPERVISOR: probe of %s rings: MC can read %d of %d pages\n", c->name, readable,
                    2 * DEMO_RING_PAGES);
    }
    return which == DEMO_CHILD_PAYLOAD && !shared_ok ? -1 : 0;
}

int demo_mc_stop(int which)
{
    struct child *c = &children[which];
    uint64_t      t0 = art_sys_rdtsc();
    int64_t       ret;
    int           index;

    if (!c->running)
        return -1;
    for (index = 0; index < order_len && order[index] != which; index++)
        ;
    if (which == DEMO_CHILD_PAYLOAD)
        mc_out.ring->arg[OUT_GENERATION] |= 1;
    ret = art_sys_kill_container((size_t)index);
    if (ret < 0) {
        demo_mc_log("SUPERVISOR: kill_container of %s: %s\n", c->name, result(ret));
        return -1;
    }
    for (; index + 1 < order_len; index++)
        order[index] = order[index + 1];
    order_len--;
    c->running = 0;
    /* The processor comes back idle. A new container needs it off. */
    ret = art_sys_cpu_offline_request(c->cpu);
    demo_mc_log("SUPERVISOR: killed %s: %lu us, cpu %u offline request: %s, tsc %lu\n", c->name,
                (unsigned long)((art_sys_rdtsc() - t0) * 1000000ull / art_clock_counter_hz()), c->cpu,
                result(ret), (unsigned long)art_sys_rdtsc());
    return 0;
}
