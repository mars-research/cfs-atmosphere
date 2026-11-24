/*
 * A minimal runtime for a C program that is the only thread of a child
 * container: program entry, kernel calls, console, and a clock. It is not the
 * runtime of rt/, which the mission-critical process uses.
 *
 * The program defines `void guest_main(uintptr_t range, size_t pages)`; the
 * arguments are the range of pages that the container was created with.
 */
#ifndef DEMO_GUEST_H
#define DEMO_GUEST_H

#include <stddef.h>
#include <stdint.h>

/* Kernel calls (atmosphere-smp/boot/vsys/src/lib.rs). */
#define NR_DEBUG_PRINT            0
#define NR_ALLOC_QUOTA_4K         2
#define NR_SCHEDULE               3
#define NR_NEW_THREAD             6
#define NR_MMAP_4K                8
#define NR_UNMAP_4K               9
#define NR_NEW_PROCESS            10
#define NR_NEW_CONTAINER          13
#define NR_SEND_EMPTY             14
#define NR_SEND_EMPTY_NO_BLOCK    16
#define NR_RECEIVE_EMPTY_NO_BLOCK 17
#define NR_SEND_ENDPOINT          22
#define NR_SEND_PAGES             24
#define NR_ALLOC_THREAD_QUOTA_4K  27
#define NR_NEW_ENDPOINT           35
#define NR_THREAD_EXIT            42
#define NR_KILL_PROCESS           43
#define NR_KILL_CONTAINER         44
#define NR_DEBUG_TSC_HZ           62

/* Results below zero: -1 and -2 are the trap layer's, -(100 + i) is the
 * kernel's result number i. */
#define SYS_ERR(i) (-(100 + (i)))
#define SYS_ERROR_NO_QUOTA          SYS_ERR(5)
#define SYS_ERROR                   SYS_ERR(8)
#define SYS_ERROR_NOQUOTA           SYS_ERR(10)
#define SYS_ERROR_INVALID_ENDPOINT  SYS_ERR(17)
#define SYS_ERROR_IPC_NO_PEER       SYS_ERR(18)

/* A kernel call with up to seven arguments. Returns the value in rax. */
long guest_syscall(long nr, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
#define guest_sys0(nr)                guest_syscall(nr, 0, 0, 0, 0, 0, 0, 0)
#define guest_sys1(nr, a)             guest_syscall(nr, (long)(a), 0, 0, 0, 0, 0, 0)
#define guest_sys2(nr, a, b)          guest_syscall(nr, (long)(a), (long)(b), 0, 0, 0, 0, 0)
#define guest_sys3(nr, a, b, c)       guest_syscall(nr, (long)(a), (long)(b), (long)(c), 0, 0, 0, 0)
#define guest_sys4(nr, a, b, c, d)    guest_syscall(nr, (long)(a), (long)(b), (long)(c), (long)(d), 0, 0, 0)

static inline uint64_t guest_rdtsc(void)
{
    return __builtin_ia32_rdtsc();
}

/* The rate of the timestamp counter in Hz, from the kernel. */
uint64_t guest_tsc_hz(void);
/* Nanoseconds since the program started. */
uint64_t guest_clock_ns(void);
/* Spins for `ns` nanoseconds. A container has no call that sleeps. */
void guest_spin_ns(uint64_t ns);
/* Ends the thread. */
void guest_exit(void) __attribute__((noreturn));

void guest_main(uintptr_t range, size_t pages);

#endif
