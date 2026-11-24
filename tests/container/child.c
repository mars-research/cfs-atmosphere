/* The child of the container test. It runs alone in a container.
 *
 * ring->arg[0]: an address in the creator's memory to read, or 0 to behave.
 * ring->arg[1]: the heartbeat count at which to read it.
 */
#include <stdio.h>

#include "demo_layout.h"
#include "demo_ring.h"
#include "guest.h"

void guest_main(uintptr_t range, size_t pages)
{
    struct demo_ring *ring   = (struct demo_ring *)range;
    uint64_t          victim = ring->arg[0];
    uint64_t          when   = ring->arg[1];

    printf("CHILD: started, range %#lx, %lu pages, counter %lu Hz\n", (unsigned long)range,
           (unsigned long)pages, (unsigned long)guest_tsc_hz());
    ring->status = 1;
    for (;;) {
        ring->heartbeat++;
        if (victim != 0 && ring->heartbeat == when) {
            printf("CHILD: reading %#lx in the creator's memory, tsc %lu\n", (unsigned long)victim,
                   (unsigned long)guest_rdtsc());
            fflush(stdout);
            ring->status = *(volatile uint64_t *)victim;
            printf("CHILD: the read returned\n");
            ring->status = 99;
        }
        guest_spin_ns(10 * 1000 * 1000);
    }
}
