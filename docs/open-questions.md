# Open questions and next steps

The port runs, but it works around several things the public Atmosphere
kernel does not have. This note records what the flight software needs from
a kernel, what the port does instead today, what we need to learn about the
concurrent version of Atmosphere, and what comes after the first milestone.

`README.md` describes the port as built. This note is about what is missing.

## What cFS needs and what it gets today

The needs come from the OSAL functions that the minimal configuration calls:
the five core services and five small applications.

| Need | Used by | Public kernel | The port today |
|---|---|---|---|
| Block until woken or until a deadline | Queue get, timed semaphore waits, task delay | Blocking receive, no timeout | The runtime runs all tasks in one kernel thread and keeps deadlines on the timestamp counter. |
| Wake another thread without blocking | Semaphore give, queue put, mutex release | `send_empty_try_schedule` wakes a receiver and switches to it. A non-blocking send exists in the verified kernel (`syscall_send_empty_no_block`), but no syscall reaches it. | Not needed inside one kernel thread. |
| Thread exit and delete | Task exit, task delete, application restart | `kernel_kill_thread` exists in the verified kernel, but no syscall reaches it. | The runtime deletes its own tasks; the executive restarts an application in the test. |
| Priorities, 0 to 255 | Task create, set priority | None. One FIFO run queue per container, at most 10 runnable threads, round-robin on the timer. | Strict priority in the runtime. |
| Mutex with priority inheritance | All core mutexes | None | In the runtime. |
| Clock in real units | Clock calls, PSP time | None | The runtime calibrates the timestamp counter against the PIT at start. This needs I/O ports, which only the first process has. |
| Periodic tick | Time bases, the 1 Hz tone, `sch_lab` | Timer interrupt for time slices only, with an uncalibrated period | A runtime task per time base that waits until the next deadline. |
| Identity of the current task | Task get id | No thread-local storage | The runtime knows which task runs. |
| Files | Startup script, tables, RAM disk | No file interface | An in-memory file system, filled from a tar archive in the boot payload. |
| Loading applications | Module load, symbol lookup | None for C | Applications are linked statically. |
| UDP | `ci_lab`, `to_lab` | Network stack in `dom0` only | Test versions of both applications. |
| Floating-point state | Mission applications | Not saved; user code is built without SSE | C code is built with general-purpose registers only. |

Two more limits of the public kernel matter for a port on kernel threads: a
CPU with no runnable thread panics in `sys_receive_empty`, and the second
processor is not started.

## Questions for the concurrent Atmosphere

The port was written against the public kernel. A newer, concurrent version
exists that is not public. Moving the tasks onto kernel threads means
replacing `atmosphere/user/atmo-rt/src/task.rs`; the synchronization objects
sit on its block, wake, and priority calls. These answers decide how.

1. Can a thread wait with a timeout? If not, what is the smallest kernel
   change that gives one?
2. Can a thread exit, and can another thread delete it?
3. Does the scheduler have priorities? How many levels?
4. Is there a calibrated clock, or does user code calibrate the timestamp
   counter?
5. How does a thread find its own identity cheaply?
6. Does the loader in `dom0` handle a large position-independent C executable
   with its relocations? The boot loader does: it starts the 4 MB cFS image as
   the first process.
7. How much memory can one process map, and what are the limits on threads
   and endpoints per process? In the public kernel a process has at most 128
   threads and a thread at most 128 endpoint descriptors.
8. Is user-space floating point planned, and where does the state save
   belong?

## Next steps

- **Tasks on kernel threads.** Depends on questions 1 to 5.
- **A Linux baseline.** Build the same configuration natively and keep its
  console log as the reference. Not done yet.
- **KVM and hardware.** Everything so far ran under QEMU software emulation.
- **Calibrate the time slice.** It is a raw timer count today.
- **Upstream the verified changes.** The timer preemption under
  `kernel/verified` has to land in the repository that the verified kernel is
  synced from.
- **Two instances.** Run two cFS instances in two processes and connect them
  with the Software Bus Network application (`apps/sbn`), with a transport
  over an Atmosphere endpoint in place of its UDP transport.
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
