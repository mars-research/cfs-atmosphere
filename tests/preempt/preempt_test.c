/* Test of timer preemption in the Atmosphere kernel.
 *
 * Two kernel threads of one process share the CPU. Neither yields, so each
 * can only run when the timer interrupt takes the CPU from the other. The
 * test covers the four ways a thread gets the CPU back:
 *
 *   1. preempted, resumed by the timer   (phase 1)
 *   2. preempted, resumed by a syscall   (phase 2: the main thread blocks)
 *   3. in a syscall, resumed by a syscall (phase 2: the worker's send)
 *   4. in a syscall, resumed by the timer (phase 3)
 *
 * While a thread spins it holds a known value in every general-purpose
 * register and in the carry flag, and checks them afterwards. Across a
 * syscall it checks the registers that a syscall preserves.
 *
 * Only the main thread calls the runtime, which is not safe for use from two
 * kernel threads. The worker thread touches a few shared words and nothing
 * else.
 */
#include <stdint.h>
#include <stdio.h>

#include "atmo_rt.h"

/* Kernel system calls (atmosphere/asys/src/lib.rs). */
#define NR_NEW_ENDPOINT     3
#define NR_NEW_THREAD       6
#define NR_RECEIVE_EMPTY    10
#define NR_SEND_EMPTY_TRY   20

#define SECOND      1000000000ull
#define SPIN_COUNT  100000      /* a fraction of a time slice under QEMU */

/* Spins `count` times with a pattern in every register. Returns the number of
 * registers, plus the carry flag, that did not keep their value. */
uint64_t spin_check(uint64_t count);

/* Makes system call `nr` with one argument. Returns the number of
 * callee-saved registers that did not keep their value. */
uint64_t syscall_check(uint64_t nr, uint64_t arg);

__asm__(
    ".intel_syntax noprefix\n"
    ".section .rodata\n"
    ".balign 8\n"
    "reg_pattern:\n"
    "  .quad 0x1111111111111111, 0x2222222222222222, 0x3333333333333333\n"
    "  .quad 0x4444444444444444, 0x5555555555555555, 0x6666666666666666\n"
    "  .quad 0x7777777777777777, 0x8888888888888888, 0x9999999999999999\n"
    "  .quad 0xaaaaaaaaaaaaaaaa, 0xbbbbbbbbbbbbbbbb, 0xcccccccccccccccc\n"
    "  .quad 0xdddddddddddddddd, 0xeeeeeeeeeeeeeeee\n"
    ".text\n"
    ".macro check_reg reg, index\n"
    "  cmp \\reg, qword ptr [rip + reg_pattern + 8*\\index]\n"
    "  je 9f\n"
    "  inc rcx\n"
    "9:\n"
    ".endm\n"

    ".globl spin_check\n"
    ".type spin_check, @function\n"
    "spin_check:\n"
    "  push rbx\n"
    "  push rbp\n"
    "  push r12\n"
    "  push r13\n"
    "  push r14\n"
    "  push r15\n"
    "  mov rcx, rdi\n"
    "  mov rax, qword ptr [rip + reg_pattern + 8*0]\n"
    "  mov rbx, qword ptr [rip + reg_pattern + 8*1]\n"
    "  mov rdx, qword ptr [rip + reg_pattern + 8*2]\n"
    "  mov rsi, qword ptr [rip + reg_pattern + 8*3]\n"
    "  mov rdi, qword ptr [rip + reg_pattern + 8*4]\n"
    "  mov rbp, qword ptr [rip + reg_pattern + 8*5]\n"
    "  mov r8,  qword ptr [rip + reg_pattern + 8*6]\n"
    "  mov r9,  qword ptr [rip + reg_pattern + 8*7]\n"
    "  mov r10, qword ptr [rip + reg_pattern + 8*8]\n"
    "  mov r11, qword ptr [rip + reg_pattern + 8*9]\n"
    "  mov r12, qword ptr [rip + reg_pattern + 8*10]\n"
    "  mov r13, qword ptr [rip + reg_pattern + 8*11]\n"
    "  mov r14, qword ptr [rip + reg_pattern + 8*12]\n"
    "  mov r15, qword ptr [rip + reg_pattern + 8*13]\n"
    "  stc\n"                   /* dec and jnz leave the carry flag alone */
    "1:\n"
    "  dec rcx\n"
    "  jnz 1b\n"
    "  sbb ecx, ecx\n"          /* -1 if the carry flag survived, else 0 */
    "  inc ecx\n"               /* errors so far: 0 or 1 */
    "  check_reg rax, 0\n"
    "  check_reg rbx, 1\n"
    "  check_reg rdx, 2\n"
    "  check_reg rsi, 3\n"
    "  check_reg rdi, 4\n"
    "  check_reg rbp, 5\n"
    "  check_reg r8, 6\n"
    "  check_reg r9, 7\n"
    "  check_reg r10, 8\n"
    "  check_reg r11, 9\n"
    "  check_reg r12, 10\n"
    "  check_reg r13, 11\n"
    "  check_reg r14, 12\n"
    "  check_reg r15, 13\n"
    "  mov rax, rcx\n"
    "  pop r15\n"
    "  pop r14\n"
    "  pop r13\n"
    "  pop r12\n"
    "  pop rbp\n"
    "  pop rbx\n"
    "  ret\n"
    ".size spin_check, . - spin_check\n"

    ".globl syscall_check\n"
    ".type syscall_check, @function\n"
    "syscall_check:\n"
    "  push rbx\n"
    "  push rbp\n"
    "  push r12\n"
    "  push r13\n"
    "  push r14\n"
    "  push r15\n"
    "  mov rax, rdi\n"
    "  mov rdi, rsi\n"
    "  xor esi, esi\n"
    "  xor edx, edx\n"
    "  mov rbx, qword ptr [rip + reg_pattern + 8*1]\n"
    "  mov rbp, qword ptr [rip + reg_pattern + 8*5]\n"
    "  mov r12, qword ptr [rip + reg_pattern + 8*10]\n"
    "  mov r13, qword ptr [rip + reg_pattern + 8*11]\n"
    "  mov r14, qword ptr [rip + reg_pattern + 8*12]\n"
    "  mov r15, qword ptr [rip + reg_pattern + 8*13]\n"
    "  syscall\n"
    "  xor ecx, ecx\n"
    "  check_reg rbx, 1\n"
    "  check_reg rbp, 5\n"
    "  check_reg r12, 10\n"
    "  check_reg r13, 11\n"
    "  check_reg r14, 12\n"
    "  check_reg r15, 13\n"
    "  mov rax, rcx\n"
    "  pop r15\n"
    "  pop r14\n"
    "  pop r13\n"
    "  pop r12\n"
    "  pop rbp\n"
    "  pop rbx\n"
    "  ret\n"
    ".size syscall_check, . - syscall_check\n"
    ".att_syntax prefix\n");

/* A system call whose result the kernel returns in rax. */
static long kernel_call(long nr, long a, long b, long c)
{
    __asm__ volatile("syscall"
                     : "+a"(nr), "+D"(a), "+S"(b), "+d"(c)
                     :
                     : "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
    return nr;
}

/* Shared between the two threads. */
static volatile uint64_t worker_rounds;
static volatile uint64_t worker_errors;
static volatile uint64_t worker_send_calls;
static volatile int      send_request;
static volatile int      main_awake;

static uint8_t worker_stack[64 * 1024] __attribute__((aligned(16)));

/* The second kernel thread. It never returns and never calls the runtime. */
static void worker(void)
{
    for (;;) {
        worker_errors += spin_check(SPIN_COUNT);
        worker_rounds++;
        if (send_request && !main_awake) {
            /* Wakes the main thread if it waits on the endpoint, and gives it
             * the CPU. Has no effect otherwise. */
            worker_send_calls++;
            worker_errors += syscall_check(NR_SEND_EMPTY_TRY, 0);
        }
    }
}

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

/* Spins until the worker has completed `rounds` more rounds, or until the
 * time limit. Returns the number of times the worker was seen to have run. */
static uint64_t spin_until_worker_ran(uint64_t rounds, uint64_t limit_ns,
                                      uint64_t *main_rounds, uint64_t *main_errors)
{
    uint64_t deadline = art_clock_ns() + limit_ns;
    uint64_t start    = worker_rounds;
    uint64_t seen     = start;
    uint64_t switches = 0;

    while (worker_rounds - start < rounds && art_clock_ns() < deadline) {
        *main_errors += spin_check(SPIN_COUNT);
        (*main_rounds)++;
        uint64_t now = worker_rounds;
        if (now != seen) {
            switches++;
            seen = now;
        }
    }
    return switches;
}

int main(void)
{
    uint64_t main_rounds = 0, main_errors = 0;

    printf("preemption test\n");

    CHECK(kernel_call(NR_NEW_ENDPOINT, 0, 0, 0) == 0);
    /* As after a call: the stack pointer is 8 below a multiple of 16. */
    uintptr_t sp = (uintptr_t)worker_stack + sizeof(worker_stack) - 8;
    CHECK(kernel_call(NR_NEW_THREAD, 0, (long)(uintptr_t)worker, (long)sp) == 0);
    if (failures != 0) {
        printf("could not start the worker thread\n");
        return 1;
    }

    /* Phase 1. Neither thread enters the kernel on its own. */
    uint64_t t0       = art_clock_ns();
    uint64_t w0       = worker_rounds;
    uint64_t switches = spin_until_worker_ran(2000, 30 * SECOND, &main_rounds, &main_errors);
    uint64_t elapsed  = art_clock_ns() - t0;
    uint64_t w_rounds = worker_rounds - w0;
    printf("phase 1: %llu ms, main %llu rounds, worker %llu rounds, "
           "worker seen running %llu times\n",
           (unsigned long long)(elapsed / 1000000), (unsigned long long)main_rounds,
           (unsigned long long)w_rounds, (unsigned long long)switches);
    CHECK(w_rounds >= 2000);
    CHECK(switches >= 20);
    CHECK(main_errors == 0);
    CHECK(worker_errors == 0);
    /* Equal time slices: neither thread gets more than three times the other. */
    CHECK(main_rounds <= 3 * w_rounds && w_rounds <= 3 * main_rounds);
    if (switches > 0)
        printf("         about %llu us between switches\n",
               (unsigned long long)(elapsed / 1000 / (2 * switches)));

    /* Phase 2. The main thread blocks on the endpoint. The worker, which lost
     * the CPU to the timer, is resumed from inside this syscall. It then sends
     * on the endpoint, which resumes the main thread from the worker's
     * syscall. */
    uint64_t sends_before = worker_send_calls;
    send_request = 1;
    uint64_t saved_errors = syscall_check(NR_RECEIVE_EMPTY, 0);
    uint64_t w1 = worker_rounds;
    main_awake = 1;
    printf("phase 2: main thread blocked and was woken, worker made %llu send calls\n",
           (unsigned long long)(worker_send_calls - sends_before));
    CHECK(saved_errors == 0);
    CHECK(worker_send_calls > sends_before);
    CHECK(worker_errors == 0);

    /* Phase 3. The worker waits in the run queue inside its send syscall. The
     * main thread does not enter the kernel, so only the timer can resume the
     * worker. */
    uint64_t rounds3 = 0, errors3 = 0;
    switches = spin_until_worker_ran(500, 30 * SECOND, &rounds3, &errors3);
    printf("phase 3: worker ran %llu more rounds after its syscall\n",
           (unsigned long long)(worker_rounds - w1));
    CHECK(worker_rounds - w1 >= 500);
    CHECK(switches >= 5);
    CHECK(errors3 == 0);
    CHECK(worker_errors == 0);

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures == 0 ? "PASS\n" : "FAIL\n");
    return failures == 0 ? 0 : 1;
}
