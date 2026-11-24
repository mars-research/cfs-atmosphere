/*
 * The mission-critical side of the demo, without cFE: the life cycle of the
 * child containers and MC's ends of its two rings.
 *
 * The cFS applications SUPERVISOR and BUS_BRIDGE are thin layers over this
 * file, and tests/demo uses it without cFS. Everything here runs in the one
 * kernel thread of the mission-critical process.
 */
#ifndef DEMO_MC_H
#define DEMO_MC_H

#include <stddef.h>
#include <stdint.h>

enum { DEMO_CHILD_BUS, DEMO_CHILD_PAYLOAD, DEMO_CHILDREN };

/* The embedding program prints the log lines. */
void demo_mc_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Maps and clears the rings. Returns 0 on success. */
int demo_mc_init(void);

/* Loads the image in the file `path` into fresh pages, creates a container
 * on it, starts its processor, and unmaps the pages that only the child may
 * see. Returns 0 on success. */
int demo_mc_start(int which, const char *path);

/* Kills the container. Returns 0 on success. */
int demo_mc_stop(int which);

int         demo_mc_running(int which);
const char *demo_mc_name(int which);
/* The counter that the child increments while it runs. */
uint64_t demo_mc_heartbeat(int which);
/* How many payloads have been started. */
uint32_t demo_mc_payload_instance(void);

/* MC's ends of its rings to and from the bus. */
int  demo_mc_send(const void *pkt, uint32_t len); /* 0, or -1 if the ring is full */
int  demo_mc_recv(void *buf);                     /* a length, 0 if empty, -1 on garbage */
void demo_mc_beat(void);

/* Tries to read the pages that MC gave away. Returns the number of pages of
 * `which` that MC can still read; see demo_mc_probe in demo_mc.c. */
uintptr_t demo_mc_private_base(int which);
size_t    demo_mc_private_pages(int which);

#endif
