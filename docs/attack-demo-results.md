# Attack demo: results

The demo of `docs/attack-demo-handoff.md` runs. Flight software (cFS) is the
mission-critical process; a message bus and a payload each run in their own
container; a supervisor in the flight software restarts the payload when it
misbehaves. All eleven attack scenarios of priorities P0 to P2 pass, in five
runs of five. D1 (DMA) and S1 (scale) are not done.

Everything ran under QEMU without KVM. Times are emulation times.

## Environment

| | |
|---|---|
| Host | Apple M5 Pro, macOS 27.0.1 |
| Emulator | QEMU 11.1.2, software emulation (TCG), `-cpu max -machine q35 -smp 4 -m 2304` |
| Guest processors | 4: cFS on 0, the bus on 1, the payload on 2; 3 stays off. QEMU ran the three on separate host threads, each near 100% of a core. |
| Kernel | `mars-research/atmosphere-smp` at `b7f0bfda`: VeriFlat `516f22f5` plus the `no-pcid` build feature and `NR_DEBUG_TSC_HZ`. Built with `kernel/no-pcid`. No kernel change was made for the demo. |
| cFS | `nasa/cFS` at `1847c75`, unmodified |
| Port | this repository at `72c0b53` plus the uncommitted demo |
| Date | 2026-11-23 |

Reproduce with `tools/build-cfs.sh` and `tools/test-attack-demo.sh` in the
nix shell. The logs and reports of the five runs are in `docs/attack-demo/`
(`run1.log`, `run1.report.txt`, ...); the build user and host in the logs
were replaced by `user@host`. `tools/attack-demo-report.py <log>` produces the
report from a log. The log lines quoted below are from run 1.

## What was built

```
root container: MC = cFS as the first process, processor 0
  apps: the five of the port + SUPERVISOR + BUS_BRIDGE + ADCS_SIM
  |
  +-- child container BUS (processor 1): demo/bus, Rust, 30 KiB ELF
  +-- child container PAYLOAD (processor 2): demo/payload, C, 17 KiB ELF
```

- **Rings** (`demo/include/demo_ring.h`, `demo/bus/src/ring.rs`): one pair of
  single-producer, single-consumer rings per process, two 4K pages each.
  Each side keeps its own count and treats the shared counts as untrusted.
- **BUS** (`demo/bus`): copies each record into private memory once, parses
  the CCSDS and cFS headers, checks the record against an allow-list
  (`policy.rs`: sender, message ID, function code, exact length, field
  limits), and forwards or drops it. The sender is the ring. It limits each
  sender to 20 records per 100 ms, and asks for a restart of the payload
  after 3 denials within 1 s or 1 s over the rate.
- **SUPERVISOR** (`apps/supervisor`, `demo/mc`): loads `bus.elf` and
  `payload.elf` from the `/cf` archive into fresh pages, creates a container
  on each, and restarts a child whose heartbeat stalls for 500 ms or that the
  bus reports.
- **BUS_BRIDGE**, **ADCS_SIM**, the payload, and the attack script in
  `CI_TEST` are as the handoff describes.
- `tests/container` and `tests/demo` run the same mechanisms without cFS.

Which pages each process maps, after setup:

| Pages | MC | BUS | PAYLOAD |
|---|---|---|---|
| MC image, heap, stacks | yes | no | no |
| BUS image and stack (272) | no | yes | no |
| MC's two rings (4) | yes | yes | no |
| Payload's two rings (4) | no | yes | yes |
| Payload image and stack (144) | no | no | yes |

MC and the payload share no page. This is the P1 unmapping fix: SUPERVISOR
gives each payload instance fresh ring pages, sends them to the bus over an
endpoint (`send_pages`), and unmaps the whole payload range.

## Scenarios

13 of 13 checks pass in each of the five runs (N and E are the checks of
normal traffic before and after the attacks; U is the unmapping test).

| ID | Result | What the log shows |
|---|---|---|
| N | pass | Payload housekeeping and pointing requests reach cFS through the bus; nothing is denied. |
| U | pass | A probe thread of MC faults on each of the 272 pages given to the bus and the 148 given to the payload, and on the 4 ring pages of each later payload: 436 probes, 436 faults. |
| K1 (read) | pass | Page fault in the payload; its thread exits; SUPERVISOR detects, kills, restarts. MC's word is unchanged. |
| K1 (write) | pass | The same for a write to MC's ring. |
| K2 | pass | Every call is refused: no endpoint descriptor, no container, no child to kill. |
| K3 | pass | The kernel refuses after 11 pages; MC telemetry and the bus keep running. |
| B1 | pass | ES restart-app and ADCS reset from the payload are denied; `SAMPLE_APP` is not restarted. |
| B2 | pass | Telemetry under `SAMPLE_APP`'s ID is denied: the sender is the ring. |
| B3 | pass | The request for 90,000 millidegrees is denied; the requests before and after arrive. |
| B4 | pass | Four kinds of malformed record are denied; the bus keeps running, and the payload's next valid record gets through. |
| B5 | pass | The rate limit drops about 1 million records in 1 s; the bus asks for a restart; MC telemetry keeps its period. |
| B6 | pass | `ADCS_SIM` never sees an angle over the limit; the 3 rewritten records that the bus read with 90,000 are denied, and the bus asks for a restart. |
| D1 | not done | |
| S1 | not done | |

### Log lines (run 1)

U:
```
SUPERVISOR: probe of BUS: MC can read 0 of the 272 pages it gave away
SUPERVISOR: probe of PAYLOAD: MC can read 0 of the 148 pages it gave away
SUPERVISOR: probe of PAYLOAD rings: MC can read 0 of 4 pages
```

K1, read of a word in MC's memory, whose address `CI_TEST` prints at start:
```
CI_TEST: a word of MC memory is at 0x8000043590, value 0x5ec4e7
PAYLOAD: K1 reading MC memory at 0x8000043590
PAYLOAD: K1 read begins, tsc 35798601000
 WARN CPU 2: Page Fault in ring 3 at rip 0xb00012a8d7; the thread exits
SUPERVISOR: detected PAYLOAD heartbeat stalled for 514 ms, tsc 36326875000
SUPERVISOR: killed PAYLOAD: 2767 us, cpu 2 offline request: ok, tsc 36330346000
PAYLOAD: started, generation 2, 148 pages at 0xb000114000, tsc 36337102000
SUPERVISOR: started PAYLOAD on cpu 2: image 26704 bytes, 148 pages given, 0 kept by MC, 6988 us, tsc 36337846000
SUPERVISOR: PAYLOAD restart 1 done in 12317 us, tsc 36339592000
CI_TEST: script done, the word of MC memory is 0x5ec4e7
```

K1, write to MC's ring:
```
PAYLOAD: K1 writing MC's ring at 0xb000110000
PAYLOAD: K1 write begins, tsc 39807507000
 WARN CPU 2: Page Fault in ring 3 at rip 0xb00012a91a; the thread exits
SUPERVISOR: detected PAYLOAD heartbeat stalled for 513 ms, tsc 40322210000
SUPERVISOR: PAYLOAD restart 2 done in 11027 us, tsc 40333643000
```

K2, reported by the payload through its ring (-117 is
`ErrorInvalidEndpoint`, -105 `ErrorNoQuota`, -145 `ErrorInvalidChild`):
```
TO_TEST: PAYLOAD_REPORT K2 send on descriptors 0..31: -117 for each
TO_TEST: PAYLOAD_REPORT K2 send_pages on descriptor 0: -117
TO_TEST: PAYLOAD_REPORT K2 send_endpoint on descriptor 0: -117
TO_TEST: PAYLOAD_REPORT K2 new_container: -105
TO_TEST: PAYLOAD_REPORT K2 kill_container(0): -145  kill_process(0): -145
```

K3 (-113 is `ErrorContainerQuotaInsufficient`):
```
TO_TEST: PAYLOAD_REPORT K3 mapped 11 pages, then mmap_4k: -105 (quota: -113)
TO_TEST: PAYLOAD_REPORT K3 spun 300 ms
```

B1, B2, B3 (the bus's lines; each denial also arrives in telemetry as a
`BUS_EVENT`):
```
BUS DENY sender=payload mid=0x1806 cc=6 reason=sender denials=2
BUS DENY sender=payload mid=0x18D1 cc=2 reason=command denials=3
BUS DENY sender=payload mid=0x0883 cc=0 reason=sender denials=4
BUS DENY sender=payload mid=0x18D1 cc=1 reason=range denials=1
ADCS_SIM: SET_POINTING roll=1000 pitch=2000 yaw=3000
ADCS_SIM: SET_POINTING roll=4000 pitch=5000 yaw=6000
```

B4: a length field larger than the record, a command cut off inside its
secondary header, a record of three bytes, and a record length that the ring
cannot hold:
```
BUS DENY sender=payload mid=0x0000 cc=0 reason=malformed denials=5
BUS DENY sender=payload mid=0x0000 cc=0 reason=malformed denials=6
BUS DENY sender=payload mid=0x0000 cc=0 reason=malformed denials=7
BUS DENY sender=payload mid=0x0000 cc=0 reason=malformed denials=8
TO_TEST: PAYLOAD_REPORT B4 sent four malformed records
```

B5, B6:
```
BUS DENY sender=payload reason=rate dropped=1
BUS RESTART_REQUEST sender=payload reason=rate denied=8 dropped=1173168 t=48713267us
SUPERVISOR: detected RESTART_REQUEST from the bus: process=2 reason=5 denials=8 dropped=1173168, tsc 48805435000
SUPERVISOR: PAYLOAD restart 3 done in 15330 us, tsc 48821238000
BUS DENY sender=payload mid=0x18D1 cc=1 reason=range denials=9
BUS DENY sender=payload mid=0x18D1 cc=1 reason=range denials=10
BUS DENY sender=payload mid=0x18D1 cc=1 reason=range denials=11
BUS RESTART_REQUEST sender=payload reason=range denied=11 dropped=1173168 t=52765241us
TO_TEST: ADCS_HK       cmd=17 err=0 roll=1000 pitch=-1000 yaw=500 accepted=17 max_abs=7000 tsc=55771584000
```

## Measurements

Emulation times, QEMU without KVM. The timestamp counter runs at 1.0 GHz and
advances in steps of 1,000 cycles (1 us), so short intervals are coarse.
Each row gives the least and the largest value of the five runs.

| Measurement | Least | Largest |
|---|---|---|
| Bus, cycles per forwarded record: median (304 records per run) | 1,000 | 1,000 |
| Bus, cycles per forwarded record: 99th percentile | 64,000 | 146,000 |
| Bus, cycles per forwarded record: largest | 188,000 | 324,000 |
| Bus, cycles per denied record: median (7 records per run) | 9,000 | 16,000 |
| K1 read: attack to SUPERVISOR's detection | 513 ms | 530 ms |
| K1 read: attack to the new payload's first line | 523 ms | 539 ms |
| K1 write: attack to detection | 514 ms | 526 ms |
| K1 write: attack to the new payload's first line | 521 ms | 533 ms |
| `kill_container` (4 restarts per run) | 0.17 ms | 2.8 ms |
| Load image, create container, hand over rings, unmap | 1.7 ms | 12.1 ms |
| Whole restart, kill to done | 6.2 ms | 16.1 ms |
| B5: start of the flood to restart done | 1,016 ms | 1,022 ms |
| B5: records dropped by the rate limit | 960,193 | 1,221,573 |
| Largest gap in `SAMPLE_APP` housekeeping over all attacks (period 1,000 ms) | 1,000 ms | 1,000 ms |
| Power-on to both containers running | 1,472 ms | 1,649 ms |
| Power-on to cFE OPERATIONAL | 1,577 ms | 1,758 ms |

- The detection time is the heartbeat threshold: 500 ms plus one poll of
  SUPERVISOR (20 ms). The response after detection is the restart, about
  10 ms.
- The cost per record does not include the log line that the bus prints for
  a denial.
- **Not measured:** the time from the attack to the kernel's fault line. The
  kernel's line has no timestamp. It is printed between the payload's line
  just before the access and SUPERVISOR's detection.

Sizes:

| | |
|---|---|
| `bus.elf` | 30,056 bytes on disk, 32,768 loaded |
| `payload.elf` | 17,080 bytes on disk, 26,704 loaded |
| cFS image with the three new apps | 1,078,912 bytes |
| BUS container, mapped pages | 272 of its own (image area 256, stack 16), 8 ring pages |
| PAYLOAD container, mapped pages | 144 of its own (image area 128, stack 16), 4 ring pages |
| PAYLOAD memory quota | 16 pages; with it the payload mapped 11 pages before the kernel refused (K3) |
| Kernel objects per container | 2 pages of 2 MiB and 9 pages of 4 KiB, by `SystemCalls.md` section 10; not measured |

Lines of code, without comments and blank lines:

| | |
|---|---|
| Bus (`main.rs`, `ring.rs`) | 597 |
| Policy table (`policy.rs`) | 56 |
| `BUS_BRIDGE` | 84 |
| `SUPERVISOR`: app 159, container life cycle (`demo_mc.c`) 269, ELF loader 93 | 521 |
| `ADCS_SIM` | 103 |
| Payload | 248 |
| Runtime of a child process (`guest.c`, `mem.c`) | 122 |
| Shared headers (rings, messages, layout) | 202 |

## What differs from the plan

- **Images are position-independent**, not linked at fixed addresses. The
  loader applies the relative relocations (`demo/loader/elf_load.c`), as the
  boot loader does for the first process. The link line of every other image
  of the port then works unchanged.
- **The payload holds no endpoint at all.** The plan gave it the bus's
  doorbell in descriptor 0. It is created with `new_container`, without an
  endpoint; everyone polls. K2 therefore finds all 32 descriptors empty.
- **The bus reports the payload's heartbeat.** After the unmapping fix MC
  cannot read the payload's ring, so the bus copies the heartbeat into the
  header of its ring to MC. A dead bus looks like a dead payload; SUPERVISOR
  checks the bus first and restarts both.
- **The probe covers every page once.** SUPERVISOR probes all pages of the
  first instance of each child and the 4 ring pages of each later payload,
  not all 148 pages after every restart. Each probed page costs one line of
  the kernel's log.
- **K1 is two scenarios**, a read of MC's memory and a write to MC's ring: a
  fault ends the payload's only thread, so one instance can try one access.
- **K2 does not try `new_process`.** A container may create processes and
  threads inside itself from its own quota; that makes no channel to
  anything outside, and the kernel allows it. The scenario covers the calls
  that would reach outside.
- **A restart of the bus restarts the payload too**, since both sides of the
  payload's rings start from empty. A bus restart was not exercised by a
  scenario.
- **No doorbell.** The bus and the payload spin on their processors.
- **Telemetry of the demo** is printed by `TO_TEST` with fields of its own,
  and every line carries the timestamp counter.

## Limitations

- **QEMU only.** No KVM, no hardware. The kernel was built with `no-pcid`,
  which no real deployment would use.
- **The debug calls of the trap layer are open to every process.** A payload
  can call `NR_DEBUG_EXIT` and stop the machine, or flood the console with
  `NR_DEBUG_PRINT`. These calls are not part of the verified kernel. The demo
  uses the print call for the lines of the bus and the payload. A flight
  build has to remove them or restrict them to the first process.
- **The bus, the bridge, the supervisor, and the loader are not verified.**
  The bus is trusted: it maps the rings of every process.
- **The policy table is written by hand**, 5 rows and 4 routes.
- **Nothing inside MC is isolated.** All cFS apps, `ADCS_SIM` included,
  share one address space.
- **MC and the bus share MC's rings**, and the bus could write anything into
  them. `BUS_BRIDGE` checks only the length of what it publishes.
- **The noninterference theorem was not applied.** After the unmapping fix
  MC and the payload map no common page, which is the condition the handoff
  names. We did not check the other assumptions of the theorem on the
  `proofs` branch against this setup, and the bus maps pages of both.
- **MC can block on the bus.** `send_pages` blocks until the bus receives.
  SUPERVISOR sends only after the bus says it is about to receive, but a bus
  that stops between that word and its receive call would block MC's only
  kernel thread. The kernel has no timed IPC.
- **Detection of a silent payload takes 500 ms.** It is a polling threshold.
  The kernel does not tell the parent that a thread of a child faulted.
- **Heartbeats can be forged.** A compromised payload that keeps
  incrementing its heartbeat and stays within the policy is not detected.
- **B6 is a race.** How many rewritten records the bus reads with the
  forbidden value depends on timing. In each run it read 3 within one
  second, which cut the payload off, and the attack ended there. The
  property shown is that no forbidden value reached `ADCS_SIM` in those
  attempts.
- **An earlier version of the log check failed one run** on B2: `CI_TEST`
  printed its command line after the payload had reacted on its own
  processor, so the bus's denial fell outside the scenario's window in the
  log. `CI_TEST` now prints before it sends. The denial itself was in that
  log.
- **One payload.** The static layout fits one. Processor 3 is unused.
- **Rates and thresholds are arbitrary:** 20 records per 100 ms, 3 denials
  per second, 500 ms.
- **The bus spins**, as does the payload: each takes a whole processor.
