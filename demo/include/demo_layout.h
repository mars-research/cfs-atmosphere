/*
 * Where the demo's containers live in the address space of the
 * mission-critical process (MC), which creates them.
 *
 * A child container maps one contiguous range of its creator's pages at the
 * same addresses:
 *
 *   BUS range:      BUS image, BUS stack, MC rings
 *   payload range:  payload rings, payload stack, payload image
 *
 * The bus also maps the payload's rings, at the same addresses. It is not
 * created with them: MC sends it the ring pages of each new payload over an
 * endpoint and then unmaps the whole payload range from its own address
 * space. After that MC shares pages only with the bus, and the payload only
 * with the bus.
 */
#ifndef DEMO_LAYOUT_H
#define DEMO_LAYOUT_H

#define DEMO_PAGE            4096ull
#define DEMO_REGION_BASE     0xb000000000ull

#define DEMO_BUS_IMAGE_PAGES 256
#define DEMO_STACK_PAGES     16
#define DEMO_RING_PAGES      2   /* one ring: a header page and a data page */
#define DEMO_PL_IMAGE_PAGES  128

/* Page offsets from DEMO_REGION_BASE. */
#define DEMO_BUS_IMAGE_PAGE  0
#define DEMO_BUS_STACK_PAGE  (DEMO_BUS_IMAGE_PAGE + DEMO_BUS_IMAGE_PAGES)
#define DEMO_MC_OUT_PAGE     (DEMO_BUS_STACK_PAGE + DEMO_STACK_PAGES) /* MC -> BUS */
#define DEMO_MC_IN_PAGE      (DEMO_MC_OUT_PAGE + DEMO_RING_PAGES)     /* BUS -> MC */
#define DEMO_PL_OUT_PAGE     (DEMO_MC_IN_PAGE + DEMO_RING_PAGES)      /* payload -> BUS */
#define DEMO_PL_IN_PAGE      (DEMO_PL_OUT_PAGE + DEMO_RING_PAGES)     /* BUS -> payload */
#define DEMO_PL_STACK_PAGE   (DEMO_PL_IN_PAGE + DEMO_RING_PAGES)
#define DEMO_PL_IMAGE_PAGE   (DEMO_PL_STACK_PAGE + DEMO_STACK_PAGES)
#define DEMO_REGION_PAGES    (DEMO_PL_IMAGE_PAGE + DEMO_PL_IMAGE_PAGES)

#define DEMO_ADDR(page)      (DEMO_REGION_BASE + (page) * DEMO_PAGE)

/* The range each child maps. */
#define DEMO_BUS_RANGE_PAGE  DEMO_BUS_IMAGE_PAGE
#define DEMO_BUS_RANGE_PAGES (DEMO_PL_OUT_PAGE - DEMO_BUS_IMAGE_PAGE)
#define DEMO_PL_RANGE_PAGE   DEMO_PL_OUT_PAGE
#define DEMO_PL_RANGE_PAGES  (DEMO_REGION_PAGES - DEMO_PL_OUT_PAGE)

/* Processors. MC runs on 0. */
#define DEMO_BUS_CPU 1
#define DEMO_PL_CPU  2

#endif
