/* See guest.h. */
#include "guest.h"

#include <string.h>

__asm__(
    ".intel_syntax noprefix\n"
    ".text\n"
    ".globl _start\n"
    ".type _start, @function\n"
    "_start:\n"
    /* rdi and rsi hold what the creator passed to new_container: the range. */
    "  and rsp, -16\n"
    "  xor ebp, ebp\n"
    "  call guest_entry\n"
    "  ud2\n"
    ".size _start, . - _start\n"

    /* long guest_syscall(nr, a1, a2, a3, a4, a5, a6, a7)
     * C:      rdi rsi rdx rcx r8 r9 [rsp+8] [rsp+16]
     * kernel: rax rdi rsi rdx r8 r9 r12     r13      */
    ".globl guest_syscall\n"
    ".type guest_syscall, @function\n"
    "guest_syscall:\n"
    "  push r12\n"
    "  push r13\n"
    "  mov rax, rdi\n"
    "  mov rdi, rsi\n"
    "  mov rsi, rdx\n"
    "  mov rdx, rcx\n"
    "  mov r12, [rsp + 24]\n"
    "  mov r13, [rsp + 32]\n"
    "  syscall\n"
    "  pop r13\n"
    "  pop r12\n"
    "  ret\n"
    ".size guest_syscall, . - guest_syscall\n"
    ".att_syntax prefix\n");

static uint64_t tsc_hz = 1000000000ull;
static uint64_t tsc_base;

uint64_t guest_tsc_hz(void)
{
    return tsc_hz;
}

uint64_t guest_clock_ns(void)
{
    uint64_t ticks = guest_rdtsc() - tsc_base;
    /* Without 128-bit division: whole seconds, then the rest. */
    return ticks / tsc_hz * 1000000000ull + ticks % tsc_hz * 1000000000ull / tsc_hz;
}

void guest_spin_ns(uint64_t ns)
{
    uint64_t until = guest_clock_ns() + ns;
    while (guest_clock_ns() < until)
        __builtin_ia32_pause();
}

void guest_exit(void)
{
    for (;;)
        guest_sys0(NR_THREAD_EXIT);
}

/* The console of the C library: the kernel prints whole lines. */
static char   line[256];
static size_t line_len;

void art_console_flush(void)
{
    if (line_len != 0) {
        guest_sys2(NR_DEBUG_PRINT, line, line_len);
        line_len = 0;
    }
}

void art_console_write(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            if (line_len == 0)
                line[line_len++] = ' ';
            art_console_flush();
        } else {
            if (line_len == sizeof(line))
                art_console_flush();
            line[line_len++] = data[i];
        }
    }
}

void guest_entry(uintptr_t range, size_t pages)
{
    long hz = guest_sys0(NR_DEBUG_TSC_HZ);
    if (hz > 0)
        tsc_hz = (uint64_t)hz;
    tsc_base = guest_rdtsc();
    guest_main(range, pages);
    art_console_flush();
    guest_exit();
}
