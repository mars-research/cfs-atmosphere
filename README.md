# cFS on Atmosphere

A port of NASA's core Flight System (cFS) to the Atmosphere microkernel on
x86-64. The flight software runs as one Atmosphere process, with unmodified
cFE, OSAL shared layer, PSP shared layer, and applications.

## Status

Tested under QEMU (software emulation) on the public Atmosphere kernel, at the
commit that the `atmosphere/` submodule pins. That tree has the C runtime and
one kernel change, timer preemption (see "Kernel change" below); the port does
not depend on the preemption.

| Check | Command | Result |
|---|---|---|
| C program prints through the kernel | `tests/hello/build.sh` | passes |
| Runtime: tasks, priorities, clock, queues | `tests/rt/build.sh` | 4,190 checks pass |
| OSAL functional tests (`osal/src/tests`) | `tools/run-osal-tests.sh` | 19 of 19 programs pass |
| cFE with five applications | `tools/test-cfs.sh` | all checks pass |
| Kernel: timer preemption of two threads | `tests/preempt/build.sh` | 14 checks pass |
| Kernel: Verus on `kernel/verified` | `tools/verify-atmosphere.sh` | 524 verified, 0 errors |

`tools/test-cfs.sh` boots the flight software and checks that the cFE reaches
its OPERATIONAL state, that a no-op command raises the command counter of
`SAMPLE_APP` in telemetry, and that the executive restarts `SAMPLE_APP` on
command.

## Layout

| Path | Contents |
|---|---|
| `atmosphere/` | Submodule: the Atmosphere kernel, `main` of `mars-research/atmosphere`. |
| `atmosphere/user/atmo-rt/` | The C runtime, in Rust: program entry, console, clock, tasks, mutexes, semaphores, queues, in-memory files. C interface in `include/atmo_rt.h`. |
| `cFS/` | Submodule: the NASA cFS bundle at `1847c75`, unmodified. |
| `libc/` | A freestanding C library: strings, `snprintf`, `strtoul`, time conversion. A heap and file streams for test programs only. |
| `osal/atmosphere/` | The OSAL per-OS layer (`OSAL_SYSTEM_OSTYPE=atmosphere`). |
| `osal/pc-atmosphere/` | The OSAL BSP (`OSAL_SYSTEM_BSPTYPE=pc-atmosphere`). |
| `psp/pc-atmosphere/` | The PSP (`CFE_SYSTEM_PSPNAME=pc-atmosphere`). The build links it into `cFS/psp/fsw/`, because the PSP build has no hook for an outside directory. |
| `atmo_defs/` | Mission definitions: targets, startup script, OSAL configuration, the `SCH_LAB` schedule. |
| `apps/ci_test`, `apps/to_test` | Command ingest from a built-in script, and telemetry output to the console. They stand in for `ci_lab` and `to_lab`, which need UDP. |
| `cmake/` | The CMake toolchain file for C code in an Atmosphere process. |
| `tools/` | Build and run scripts. |
| `tests/` | The hello program, the runtime tests, the kernel preemption test, and the project that builds the OSAL tests. |
| `flake.nix` | The development shell, and Verus as a package at the revision the kernel uses. |
| `docs/open-questions.md` | What the port still needs from the kernel, the questions that decide the next design, and the next steps. |

The two submodules pin the tested combination. `ATMO_ROOT` and `CFS_ROOT`
point the scripts at other checkouts.

## Build and run

Get the submodules, then enter the development shell:

    git submodule update --init --recursive
    nix develop

The shell provides rustup, a clang cross compiler (`atmo-cc`), CMake, nasm,
GNU `ld` and `objcopy` for x86-64 ELF, `xxd`, and QEMU. It is tested on macOS
(Apple silicon); it also declares Linux, which is not tested yet. Rust itself
comes from rustup, which reads
`atmosphere/rust-toolchain.toml` (Rust 1.99.1). Without nix, put the same
tools on `PATH` and set `ATMO_CC` to a clang with the x86-64 backend.

Then:

    tools/build-atmosphere.sh      # loader, kernel, runtime
    tools/build-cfs.sh             # flight software: out/cfs/cfs-cpu1.img
    tools/test-cfs.sh              # boot it and check the console output

    tests/rt/build.sh && tools/run-qemu.sh out/rt/rt_test.elf
    tests/preempt/build.sh && tools/run-qemu.sh out/preempt/preempt_test.elf
    tools/verify-atmosphere.sh     # Verus on the kernel; the first run builds Verus (nix build .#verus)

    cmake -S tests/osal -B out/osal-tests \
          -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-x86_64-atmosphere.cmake \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build out/osal-tests -j8
    tools/run-osal-tests.sh

`tools/run-qemu.sh <image> [seconds]` boots any image as the first Atmosphere
process and exits with the code that the image passed to `art_exit()`. Set
`ATMO_QEMU_ACCEL=kvm` on a Linux host and `ATMO_QEMU_RAW=1` for the whole
serial log.

## How it works

**One process, booted as the first process.** The Atmosphere loader already
loads a position-independent ELF as the first process. The image is that ELF.
A tar archive of the `/cf` volume (startup script, table files) follows it in
the boot module; the kernel passes it to the process as the boot payload, and
the PSP registers its files in the runtime's in-memory file system.

**The runtime schedules the tasks.** The public kernel runs one processor,
has no thread priorities, and cannot wake a chosen thread or wake a thread
after a timeout. The runtime therefore runs all tasks inside one kernel
thread: strict priority with 256 levels, mutexes with priority inheritance,
waits with deadlines on the timestamp counter. It needs only `mmap` and the
console call from the kernel.

The synchronization objects (`src/sync.rs`) use only the block, wake, and
priority calls of the scheduler (`src/task.rs`). A scheduler on kernel threads
can replace `task.rs` behind those calls when the kernel can block a thread
with a timeout.

**Static applications.** Applications are linked into the cFE executable and
listed in `cpu1_STATIC_APPLIST` and `cpu1_STATIC_SYMLIST` in
`atmo_defs/targets.cmake`. The cFE name in each symbol pair must match the
fourth field of the application's line in the startup script.

**General-purpose registers only.** The kernel does not save vector or
floating-point state, so all C code is compiled with `-mgeneral-regs-only`.
Code that uses `float` or `double` does not compile.

## Kernel change

The kernel now ends a thread's time slice on the local APIC timer and runs
the next thread of the container's run queue (round-robin, no priorities).

Verified part (`kernel/verified`, checked by `tools/verify-atmosphere.sh`):

- `kernel/kernel_timer_interrupt.rs`: `Kernel::timer_interrupt`, the entry
  point. It preserves the kernel invariant and either changes nothing or
  switches threads as `ProcessManager::preempt_spec` states: the thread that
  ran goes to the tail of the run queue and the head of the queue runs.
- `process_manager/impl_preempt.rs`: `preempt_running_thread`, and the two
  functions that save and hand back the whole context of a preempted thread.
  They are verified code, not new trusted setters.
- `trap.rs`: the trap frame records whether it holds the whole context
  (`preempted`). The existing run-queue functions save and restore only the
  registers that a syscall preserves.
- `kernel/schedule_idle_cpu.rs`: a preempted thread gets every register back,
  and the return value says so (`SwitchDecision::SwitchPreempted`).
- `process_manager/impl_base.rs`: `pop_scheduler_for_idle_cpu` states which
  thread it returns. One existing proof, `set_container_mem_quota_mem_4k`,
  needed twice the default solver limit after the trap frame grew by a field.

Unverified part (`kernel/src`):

- `syscalls/mod.rs`: syscalls run with interrupts off (`IA32_FMASK`). The two
  selector bases in `STAR` were swapped, so user code ran with the selector of
  a data descriptor in CS; `sysret` does not check this, but the `iret` of an
  interrupt does.
- `gdt/mod.rs`: kernel code now precedes kernel data, the order `syscall`
  expects.
- `kernel/mod.rs`: `timer_preempt` calls the verified entry point and loads
  the address space; `sys_receive_empty` leaves through the `iret` path when
  the next thread was preempted.
- `main.rs`, `thread.rs`: the timer is armed before the first process starts.

The verified kernel is synced from another repository, so the changes under
`kernel/verified` have to land there to survive the next sync.

Not done: the time slice is an uncalibrated timer count (about 1.3 ms
under QEMU); a thread keeps the CPU when the run queue is full (10 entries);
vector and floating-point state is not saved; a CPU with no runnable thread
still panics in `sys_receive_empty`; the second processor is not started.

## Limits

- The runtime does not preempt its tasks. A task that computes for a long
  time without an OSAL call delays every other task, timers included.
  `art_poll()` lets such a task give way. The kernel now preempts kernel
  threads, but all tasks share one kernel thread.
- Tested under QEMU software emulation only, not under KVM or on hardware.
- Two checks in `tests/rt` bound wall-clock times (a sleep within 1 ms, the
  clock within 2%). They fail now and then when the host is busy, on the
  original kernel as well.
- Clock calibration and the QEMU exit device use I/O ports, which only the
  first process may access.
- No network, no dynamic loading, no floating point. Condition variables and
  readers-writer locks return "not implemented".
- The local clock starts at zero; nothing reads a real-time clock.
- Files are in memory and do not survive a restart, so every start is a
  power-on reset.
- The executive checks that an application's module file exists before a
  restart. `tools/build-cfs.sh` adds an empty `<app>.so` for each application.
- The Linux baseline of the same configuration has not been run.

## License

Apache-2.0, see `LICENSE`. The files derived from NASA cFS keep their notices.
The submodules have their own licenses: Atmosphere is MIT, cFS is Apache-2.0.
