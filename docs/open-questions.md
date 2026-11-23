# Open questions and next steps

The port runs on VeriFlat, the concurrent Atmosphere kernel, but still
schedules its tasks itself inside one kernel thread. This note records what
the flight software needs from a kernel, what VeriFlat offers, what the port
does instead today, and what comes next.

`README.md` describes the port as built. This note is about what is missing.
The kernel facts are from `atmosphere-smp/SystemCalls.md` and `atmosphere-smp/boot` at the
commit that the submodule pins.

## What cFS needs and what it gets today

The needs come from the OSAL functions that the minimal configuration calls:
the five core services and five small applications.

| Need | Used by | The kernel | The port today |
|---|---|---|---|
| Block until woken or until a deadline | Queue get, timed semaphore waits, task delay | Blocking send, receive, and call on an endpoint. No timeout on any of them, and no sleep call. | The runtime runs all tasks in one kernel thread and keeps deadlines on the timestamp counter. A waiting runtime spins. |
| Wake another thread without blocking | Semaphore give, queue put, mutex release | `send_empty_no_block` makes the receiver at the head of an endpoint queue runnable; the sender keeps the processor. The waker must hold the endpoint in one of its own 32 descriptors. | Not needed inside one kernel thread. |
| Thread exit and delete | Task exit, task delete, application restart | `thread_exit` ends the calling thread. Nothing ends another thread of the same process; `kill_process` ends a child process with all its threads. | The runtime deletes its own tasks; the executive restarts an application in the test. |
| Priorities, 0 to 255 | Task create, set priority | None. Each container has one FIFO run queue per processor, served round-robin on the timer; an idle processor pulls from another queue. | Strict priority in the runtime. |
| Mutex with priority inheritance | All core mutexes | None | In the runtime. |
| Clock in real units | Clock calls, PSP time | None upstream. A process has no I/O ports, so it cannot calibrate the timestamp counter itself. | Our kernel clone adds a trap-layer call that reports the counter rate, which the kernel measures against the interval timer at boot. |
| Periodic tick | Time bases, the 1 Hz tone, `sch_lab` | A timer interrupt for time slices only, as a raw timer count. | A runtime task per time base that waits until the next deadline. |
| Identity of the current task | Task get id | No thread-local storage: the `fs` and `gs` bases are not per thread. `rdtscp` gives the processor number. | The runtime knows which task runs. |
| Files | Startup script, tables, RAM disk | No file interface | An in-memory file system, filled from a tar archive in the boot payload. |
| Loading applications | Module load, symbol lookup | The loader relocates one ELF, the first process. `new_process` starts a process on pages that its parent already maps. | Applications are linked statically. |
| UDP | `ci_lab`, `to_lab` | No network stack on `main`; the `bench` branch has device drivers. | Test versions of both applications. |
| Floating-point state | Mission applications | Turned off for user code: the instruction faults and the thread exits. | C code is built with general-purpose registers only. |

## What the concurrent kernel answers

The earlier version of this note asked eight questions about the concurrent
kernel. The answers:

1. **Can a thread wait with a timeout?** No. This is the main gap.
2. **Can a thread exit, and can another thread delete it?** It can exit.
   Another thread cannot delete it; a parent can kill a child process.
3. **Does the scheduler have priorities?** No.
4. **Is there a calibrated clock?** Not upstream; our kernel clone adds the
   call.
5. **How does a thread find its identity?** Not through the kernel. The
   stack address can serve: each task has its own stack.
6. **Does the loader handle the cFS image?** Yes. The 4 MB
   position-independent image starts as the first process.
7. **Limits?** The kernel manages the first 4 GiB of memory and up to 28
   processors. A thread has 32 endpoint descriptors. The loader reserves
   64 MiB for the first process: image, 16 MiB stack, and payload. Memory
   beyond that is mapped with `mmap_4k`, paid from the thread's quota.
8. **Floating point?** Not yet. The trap layer has a plan in a comment:
   a per-thread flag, and save and restore on switches that involve a thread
   that uses it.

## Tasks on kernel threads

Moving the tasks onto kernel threads means replacing `rt/src/task.rs`; the
synchronization objects sit on its block, wake, and priority calls. With the
kernel as it is, three things stand in the way:

- **No timed wait.** A blocked thread wakes only when another thread sends
  to its endpoint. A timer thread in the runtime could send the wake-ups,
  but it has nothing to sleep on itself, so it would spin or yield in a loop.
- **No priorities.** The runtime would have to keep every task but the most
  urgent ready ones blocked, i.e., it would still be the scheduler, now with
  a kernel call for every switch.
- **Wake-ups need descriptors.** A thread wakes another through an endpoint
  that both hold. With one endpoint per task and 32 descriptors per thread,
  a task cannot hold the endpoints of all the others. A shared endpoint
  wakes its queue head, not a chosen thread.

Each of these is a change to the verified kernel, so they are questions for
its authors: a receive with a deadline, a priority per thread with one run
queue per level, and a wake that names a thread.

Until then, kernel threads are still useful in two places that need neither:
running a second cFS instance in a second process, and running a device or
network server next to the flight software.

## Next steps

- **A timed wait in the kernel.** Decide its form with the kernel authors.
  The rest of the move to kernel threads depends on it.
- **Send the two kernel commits upstream.** They are on `main` of
  `mars-research/atmosphere-smp`. The TLB shootdown test in
  `docs/tlb-shootdown-test.diff` could go with them.
- **KVM and hardware.** Everything so far ran under QEMU software emulation,
  with the `no-pcid` kernel.
- **A Linux baseline.** Build the same configuration natively and keep its
  console log as the reference. Not done yet.
- **Give the processor back.** A waiting runtime spins. Until the kernel has
  a timed wait it could at least yield.
- **Two instances.** Run two cFS instances in two processes and connect them
  with the Software Bus Network application (`apps/sbn`), with a transport
  over an endpoint in place of its UDP transport.
- **Floating point.** Mission applications such as attitude control need it,
  so the kernel has to save and restore that state.
- **A verified software bus.** Later.

## Related work

Furgala, Jero, Lin, and Skowyra ported cFS to seL4: "Porting NASA's core
Flight System to the Formally Verified seL4 Microkernel," SpaceSec 2026
([paper](https://www.ndss-symposium.org/ndss-paper/porting-nasas-core-flight-system-to-the-formally-verified-sel4-microkernel)).
Three of their findings shaped this port:

- **cFS creates and destroys its resources at run time.** Their first design
  described the system statically and did not fit, so they built a new
  operating system layer on seL4. Our runtime creates every object
  dynamically.
- **Mutexes need priority inheritance.** They implement it and call it a
  feature that real-time systems depend on. Our runtime implements it too.
- **A minimal configuration is enough to start.** Theirs was the five core
  services, the housekeeping application, and test versions of command ingest
  and telemetry output, which they found needs about 36 kinds of operating
  system function. Ours adds `sample_app` and `sch_lab` to that set.

They report that their first version took three developers almost a year and
a half.
