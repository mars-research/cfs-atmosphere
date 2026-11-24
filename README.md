# cFS on Atmosphere

A port of NASA's core Flight System (cFS) to the Atmosphere microkernel on
x86-64. The flight software runs as one Atmosphere process, with unmodified
cFE, OSAL shared layer, PSP shared layer, and applications.

The kernel is the concurrent version of Atmosphere,
`mars-research/atmosphere-smp`. It is a clone of VeriFlat
(`FeizaiYiHao/VeriFlat`), the name the kernel has in its sources. The port first ran on the earlier, single-processor kernel
(`mars-research/atmosphere`); the last commit of this repository on that
kernel is `3e399dc`.

## Status

Tested under QEMU (software emulation) on the kernel commit that the
`atmosphere-smp/` submodule pins: upstream `516f22f5` plus two commits (see
"Kernel" below).

| Check | Command | Result |
|---|---|---|
| C program prints through the kernel | `tests/hello/build.sh` | passes |
| Runtime: tasks, priorities, clock, queues | `tests/rt/build.sh` | 4,191 checks pass |
| OSAL functional tests (`osal/src/tests`) | `tools/run-osal-tests.sh` | 19 of 19 programs pass |
| cFE with five applications | `tools/test-cfs.sh` | all checks pass |
| Kernel: two threads, preemption and thread switches | `tests/preempt/build.sh` | 20 checks pass |
| Kernel: its own self-test (`boot/dom0`), 4 processors | `atmosphere-smp/boot/run.sh` | passes |
| Child containers: load, isolate, fault, kill, restart | `tests/container/build.sh` | 19 checks pass |
| Attack demo without cFS: bus and payload in containers | `tests/demo/build.sh` | 38 checks pass |
| Attack demo under cFS, 4 processors | `tools/test-attack-demo.sh` | 13 of 13 scenarios pass |
| Kernel: Verus | `tools/verify-atmosphere.sh` | 5017 verified, 0 errors |

`tools/test-cfs.sh` boots the flight software and checks that the cFE reaches
its OPERATIONAL state, that a no-op command raises the command counter of
`SAMPLE_APP` in telemetry, and that the executive restarts `SAMPLE_APP` on
command.

## Layout

| Path | Contents |
|---|---|
| `atmosphere-smp/` | Submodule: the kernel, `main` of `mars-research/atmosphere-smp`. Its `boot/` directory has the loader, the trap layer, and the syscall stubs (`vsys`). |
| `rt/` | The C runtime, in Rust: program entry, console, clock, tasks, mutexes, semaphores, queues, in-memory files. C interface in `include/atmo_rt.h`. Only `src/sys.rs` makes system calls. |
| `cFS/` | Submodule: the NASA cFS bundle at `1847c75`, unmodified. |
| `libc/` | A freestanding C library: strings, `snprintf`, `strtoul`, time conversion. A heap and file streams for test programs only. |
| `osal/atmosphere/` | The OSAL per-OS layer (`OSAL_SYSTEM_OSTYPE=atmosphere`). |
| `osal/pc-atmosphere/` | The OSAL BSP (`OSAL_SYSTEM_BSPTYPE=pc-atmosphere`). |
| `psp/pc-atmosphere/` | The PSP (`CFE_SYSTEM_PSPNAME=pc-atmosphere`). The build links it into `cFS/psp/fsw/`, because the PSP build has no hook for an outside directory. |
| `atmo_defs/` | Mission definitions: targets, startup script, OSAL configuration, the `SCH_LAB` schedule. |
| `apps/ci_test`, `apps/to_test` | Command ingest from a built-in script, and telemetry output to the console. They stand in for `ci_lab` and `to_lab`, which need UDP. |
| `apps/supervisor`, `apps/bus_bridge`, `apps/adcs_sim` | The flight-software side of the attack demo: the loader and watchdog of the containers, the gateway to the bus, and a stand-in for attitude control. |
| `demo/` | The rest of the attack demo: the bus (Rust) and the payload (C), which run in child containers, the shared ring and message definitions, the ELF loader, and the container life cycle (`demo/mc`). |
| `cmake/` | The CMake toolchain file for C code in an Atmosphere process. |
| `tools/` | Build and run scripts. |
| `tests/` | The hello program, the runtime tests, the kernel thread test, the container test, the attack demo without cFS, and the project that builds the OSAL tests. |
| `flake.nix` | The development shell. |
| `docs/open-questions.md` | What the port still needs from the kernel, the questions that decide the next design, and the next steps. |
| `docs/attack-demo-results.md` | The attack demo: what was built, the scenarios, the log lines, the measurements, and the limitations. `docs/attack-demo/` has the logs. |

The two submodules pin the tested combination. `ATMO_ROOT` (the kernel tree)
and `CFS_ROOT` point the scripts at other checkouts.

## Build and run

Get the submodules, then enter the development shell:

    tools/get-submodules.sh
    nix develop

`tools/get-submodules.sh` fetches the kernel, a shallow copy of the Verus
inside it (the build needs its `vstd` sources), and cFS. A recursive
submodule update works too, but fetches the whole history of Verus.

The shell provides rustup, a clang cross compiler (`atmo-cc`), CMake, nasm,
GNU `ld` and `objcopy` for x86-64 ELF, `xxd`, and QEMU. It is tested on macOS
(Apple silicon); it also declares Linux, which is not tested yet. Rust itself
comes from rustup; the scripts select the toolchain that
`atmosphere-smp/boot/rust-toolchain.toml` names (Rust 1.99.1). Without nix, put the
same tools on `PATH` and set `ATMO_CC` to a clang with the x86-64 backend.

Then:

    tools/build-atmosphere.sh      # loader, kernel, runtime
    tools/build-cfs.sh             # flight software: out/cfs/cfs-cpu1.img
    tools/test-cfs.sh              # boot it and check the console output
    tools/test-attack-demo.sh      # the same boot, checked for the attack demo

    tests/rt/build.sh && tools/run-qemu.sh out/rt/rt_test.elf
    tests/preempt/build.sh && tools/run-qemu.sh out/preempt/preempt_test.elf
    tools/verify-atmosphere.sh     # Verus on the kernel; the first run builds Verus

    cmake -S tests/osal -B out/osal-tests \
          -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-x86_64-atmosphere.cmake \
          -DCMAKE_BUILD_TYPE=Release
    cmake --build out/osal-tests -j8
    tools/run-osal-tests.sh

`tools/run-qemu.sh <image> [seconds]` boots any image as the first process and
exits with the code that the image passed to `art_exit()`. It prints what the
image wrote and the kernel's warnings; `ATMO_QEMU_RAW=1` prints the whole
serial log. `ATMO_QEMU_SMP=4` gives the machine four processors.

QEMU without KVM emulates neither PCID nor INVPCID, which the kernel needs,
so by default the scripts build the kernel with its feature `no-pcid`. On a
Linux host with KVM, set `ATMO_QEMU_ACCEL=kvm` for both the build and the
run; the kernel is then built without the feature. This path is not tested
yet.

## How it works

**One process, booted as the first process.** The loader already loads a
position-independent ELF as the first process (`dom0`). The image is that
ELF. A tar archive of the `/cf` volume (startup script, table files) follows
it in the boot module; the kernel passes it to the process as the boot
payload, and the PSP registers its files in the runtime's in-memory file
system.

**The runtime schedules the tasks.** The kernel has threads, runs them on
several processors, and preempts them on a timer. It schedules them
round-robin without priorities, and it cannot wake a thread after a timeout.
The runtime therefore still runs all tasks inside one kernel thread: strict
priority with 256 levels, mutexes with priority inheritance, waits with
deadlines on the timestamp counter. From the kernel it needs the console
call, the exit call, `mmap_4k` with the two quota calls, and the rate of the
timestamp counter.

The synchronization objects (`rt/src/sync.rs`) use only the block, wake, and
priority calls of the scheduler (`rt/src/task.rs`). A scheduler on kernel
threads can replace `task.rs` behind those calls.
`docs/open-questions.md` says what the kernel offers for that and what is
missing.

**Memory is paid for with quota.** A thread pays for the pages it maps, and
for the page tables under them, from its own quota, which it takes from its
process and its container. The kernel has no call that reads the quota, so
`rt/src/sys.rs` keeps count.

**Static applications.** Applications are linked into the cFE executable and
listed in `cpu1_STATIC_APPLIST` and `cpu1_STATIC_SYMLIST` in
`atmo_defs/targets.cmake`. The cFE name in each symbol pair must match the
fourth field of the application's line in the startup script.

**General-purpose registers only.** The kernel turns floating point off for
user code: an x87, SSE, or AVX instruction faults and the thread exits. All C
code is compiled with `-mgeneral-regs-only`, so code that uses `float` or
`double` does not compile. The runtime is built for the Rust target
`x86_64-unknown-none`, which has no SSE either.

## Attack demo

The image that `tools/build-cfs.sh` builds also holds a demonstration of
containers as a security boundary. `SUPERVISOR`, a cFS app, loads a message
bus and a payload from the `/cf` volume, each into a child container with a
processor of its own. Every message between the flight software and the
payload goes through the bus, which checks it against an allow-list. The
script of `CI_TEST` then tells the payload to attack: read and write memory
that is not its own, reach around the bus with kernel calls, exhaust its
memory, send forbidden, forged, out-of-range, and malformed messages, flood
the bus, and rewrite a message after the bus has checked it. The kernel or
the bus stops each attempt, and `SUPERVISOR` restarts the payload without an
operator. `docs/attack-demo-results.md` has the design, the evidence, the
numbers, and the limitations; all of it ran under QEMU only.

The image needs four processors, so `tools/test-cfs.sh` boots with four too.

## Kernel

The submodule is `mars-research/atmosphere-smp`, a clone of
`FeizaiYiHao/VeriFlat` (the remote `upstream` in `atmosphere-smp/`). Its
`main` is upstream `main` plus two commits to trusted code:

- `no-pcid`, a build feature for QEMU without KVM, which has no PCID and no
  INVPCID. Every CR3 load then writes the page-table root alone and flushes
  the TLB; a flush of one PCID becomes a CR3 reload and a flush of one
  address `invlpg`. The TLB holds a subset of what it holds with PCIDs. The
  default build keeps its code. The one change under `src/` is a `cfg` in the
  body of `CpuTLB::write_cr3_pcid`, which Verus does not check.
- `NR_DEBUG_TSC_HZ`, a trap-layer call that reports the rate of the timestamp
  counter. A process has no I/O ports and cannot measure it. The kernel
  measures the rate against the interval timer at boot when CPUID does not
  report it; before, it assumed 3 GHz, which is 3x off under QEMU.

Neither commit touches a specification, a proof, or verified code, and the
kernel verifies as before. They are not upstream yet.

The kernel's `dom0` self-test never causes a remote TLB flush.
`docs/tlb-shootdown-test.diff` is a test that does: a thread on another
processor reads a page while the first thread unmaps and remaps it. It was
used to check the NMI flush path of the `no-pcid` build and is not part of
the kernel tree.

`tests/preempt` runs two kernel threads of one process. A thread loses the
processor to the timer or inside a system call and gets it back from the
timer or from the other thread's system call; the test covers the four
combinations and checks every general-purpose register across each.

## Limits

- The runtime does not preempt its tasks. A task that computes for a long
  time without an OSAL call delays every other task, timers included.
  `art_poll()` lets such a task give way. The kernel preempts kernel threads,
  but all tasks share one kernel thread, on one processor.
- Tested under QEMU software emulation only, with the `no-pcid` kernel. The
  default build needs KVM or hardware, where the port has not run.
- One check in `tests/rt` bounds a wall-clock time (a sleep within 1 ms). It
  can fail when the host is busy.
- Nothing checks the clock against a second clock. A process has no I/O
  ports, so the earlier comparison with the real-time clock is gone; the
  runtime trusts the rate that the kernel measured.
- A sleeping runtime spins; it does not give the processor back to the
  kernel.
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
The submodules have their own licenses: the kernel is MIT, cFS is Apache-2.0.
