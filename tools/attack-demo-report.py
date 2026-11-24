#!/usr/bin/env python3
"""Checks the console log of the attack demo and prints the measurements.

    tools/attack-demo-report.py <log>

The log is what tools/run-qemu.sh prints for out/cfs/cfs-cpu1.img with four
processors. Each scenario is checked on the lines between the command that
starts it and the next command. Times come from the timestamp counter values
that the components print; under QEMU without KVM they are emulation times.
Exits with 0 if every scenario passes.
"""
import re
import sys

LIMIT = 30000  # millidegrees: DEMO_ADCS_MAX_MILLIDEG

lines = open(sys.argv[1], errors="replace").read().splitlines()

hz = None
for line in lines:
    m = re.search(r"SUPERVISOR: started, tsc \d+, counter (\d+) Hz", line)
    if m:
        hz = int(m.group(1))
if hz is None:
    sys.exit("no SUPERVISOR start line: this is not a log of the attack demo")


def tsc(line):
    """The timestamp counter value at the end of a line, or None."""
    m = re.search(r"tsc[= ](\d+)\s*$", line)
    return int(m.group(1)) if m else None


def ms(ticks):
    return ticks * 1000.0 / hz


# The windows: one per ATTACK command, and the time before the first one.
starts = [(i, int(m.group(1))) for i, line in enumerate(lines)
          for m in [re.search(r"CI_TEST: sending ATTACK .*\(attack=(\d+)\)", line)] if m]
end = next((i for i, line in enumerate(lines) if "CI_TEST: script done" in line), len(lines))
windows = {}
for k, (i, n) in enumerate(starts):
    stop = starts[k + 1][0] if k + 1 < len(starts) else end
    windows[n] = lines[i:stop]
before = lines[:starts[0][0]] if starts else lines
after = lines[starts[-1][0]:] if starts else []

results = []  # (id, attack, what must happen, passed, evidence lines)


def grep(pattern, where):
    return [line for line in where if re.search(pattern, line)]


def scenario(ident, title, checks, evidence):
    passed = all(ok for ok, _ in checks)
    results.append((ident, title, passed, [what for ok, what in checks if not ok], evidence))


def max_gap(where, pattern="TO_TEST: SAMPLE_APP_HK"):
    stamps = [tsc(line) for line in grep(pattern, where)]
    stamps = [t for t in stamps if t is not None]
    gaps = [b - a for a, b in zip(stamps, stamps[1:])]
    return ms(max(gaps)) if gaps else None


def restart_times(w, begin_pattern):
    """Milliseconds from the payload's own 'begins' line to each step of the response."""
    t0 = next((tsc(l) for l in w if re.search(begin_pattern, l)), None)
    detect = next((tsc(l) for l in w if "SUPERVISOR: detected" in l), None)
    done = next((tsc(l) for l in w if re.search(r"SUPERVISOR: PAYLOAD restart \d+ done", l)), None)
    started = next((tsc(l) for l in w if "PAYLOAD: started, generation" in l), None)
    if None in (t0, detect, done, started):
        return None
    return ms(detect - t0), ms(started - t0), ms(done - t0)


def pointing_values(where):
    out = []
    for line in grep(r"ADCS_SIM: SET_POINTING", where):
        out += [abs(int(v)) for v in re.findall(r"=(-?\d+)", line)]
    return out


# ---- Normal traffic ----
scenario("N", "normal traffic before the attacks", [
    (len(grep(r"TO_TEST: IMAGER_HK", before)) >= 5, "payload housekeeping reaches MC telemetry"),
    (len(grep(r"ADCS_SIM: SET_POINTING", before)) >= 2, "pointing requests reach ADCS_SIM"),
    (len(grep(r"TO_TEST: BUS_HK", before)) >= 5, "bus housekeeping reaches MC telemetry"),
    (not grep(r"BUS DENY", before), "nothing is denied"),
], grep(r"SUPERVISOR: started (BUS|PAYLOAD)", before) + grep(r"TO_TEST: IMAGER_HK", before)[:1]
   + grep(r"ADCS_SIM: SET_POINTING", before)[:1])

# ---- Isolation of memory ----
# The payload runs on processor 2; the probe threads of MC do not.
probe_faults = len([l for l in grep(r"Page Fault in ring 3 .* the thread exits", lines) if "CPU 2:" not in l])
bus_probe = next((re.search(r"probe of BUS: MC can read (-?\d+) of the (\d+) pages", l) for l in lines
                  if "probe of BUS:" in l), None)
pl_probe = next((re.search(r"probe of PAYLOAD: MC can read (-?\d+) of the (\d+) pages", l) for l in lines
                 if "probe of PAYLOAD:" in l), None)
ring_probes = [int(m.group(1)) for l in lines
               for m in [re.search(r"probe of PAYLOAD rings: MC can read (-?\d+) of", l)] if m]
probed = (int(bus_probe.group(2)) if bus_probe else 0) + (int(pl_probe.group(2)) if pl_probe else 0) \
    + 4 * len(ring_probes)
scenario("U", "MC unmaps what it gave to the containers", [
    (bus_probe is not None and int(bus_probe.group(1)) == 0, "MC cannot read the bus's image or stack"),
    (pl_probe is not None and int(pl_probe.group(1)) == 0, "MC cannot read the payload's image, stack, or rings"),
    (bool(ring_probes) and all(n == 0 for n in ring_probes), "nor the rings of any later payload"),
    (probe_faults == probed, "each probed page gave one page fault of a probe thread"),
], grep(r"SUPERVISOR: probe of", lines)[:3])
results[-1] += ((probe_faults, probed),)

# ---- K1 ----
for n, ident, verb in ((1, "K1a", "read"), (2, "K1b", "write")):
    w = windows.get(n, [])
    times = restart_times(w, rf"PAYLOAD: K1 {verb} begins")
    gap = max_gap(w)
    scenario(ident, f"payload {verb}s memory it does not map", [
        (bool(grep(r"CPU 2: Page Fault in ring 3 .* the thread exits", w)), "the kernel faults the payload's thread"),
        (not grep(r"K1 (READ|WRITE) RETURNED", lines), "the access does not return"),
        (bool(grep(r"SUPERVISOR: detected PAYLOAD heartbeat stalled", w)), "SUPERVISOR notices the stalled heartbeat"),
        (bool(grep(r"SUPERVISOR: killed PAYLOAD", w)), "SUPERVISOR kills the container"),
        (bool(grep(r"PAYLOAD: started, generation", w)), "a new payload starts"),
        (gap is not None and gap < 1500, "MC telemetry keeps its period"),
    ], grep(r"PAYLOAD: K1|CPU 2: Page Fault|SUPERVISOR: (detected|killed|started|PAYLOAD restart)|PAYLOAD: started", w))
    results[-1] += ((times, gap),)

# ---- K2 ----
w = windows.get(3, [])
reports = grep(r"TO_TEST: PAYLOAD_REPORT K2", w)
codes = [int(c) for line in reports for c in re.findall(r": (-?\d+)", line)]
scenario("K2", "payload tries to reach around the bus", [
    (len(reports) == 5, "the payload reports five attempts through its ring"),
    (bool(grep(r"descriptors 0\.\.31: -117 for each", reports)), "no endpoint descriptor exists"),
    (bool(codes) and all(c < 0 for c in codes), "every call is refused"),
    (not grep(r"SUPERVISOR: restarting", w), "the payload keeps running"),
], reports)

# ---- K3 ----
w = windows.get(4, [])
gap = max_gap(w)
m = next((re.search(r"K3 mapped (\d+) pages, then mmap_4k: (-?\d+)", l) for l in grep(r"PAYLOAD_REPORT K3 mapped", w)), None)
scenario("K3", "payload takes all the memory it can and spins", [
    (m is not None and int(m.group(2)) == -105, "the kernel refuses with no quota"),
    (gap is not None and gap < 1500, "MC telemetry keeps its period"),
    (len(grep(r"TO_TEST: BUS_HK", w)) >= 2, "the bus keeps running"),
], grep(r"PAYLOAD_REPORT K3", w))
results[-1] += ((None, gap),)

# ---- B1 ----
w = windows.get(5, [])
scenario("B1", "payload sends commands it may not send", [
    (bool(grep(r"BUS DENY sender=payload mid=0x1806 cc=6 reason=sender", w)), "ES restart-app is denied"),
    (bool(grep(r"BUS DENY sender=payload mid=0x18D1 cc=2 reason=command", w)), "ADCS reset is denied"),
    (not grep(r"Restart Application SAMPLE_APP", w), "SAMPLE_APP is not restarted"),
    (not grep(r"ADCS_SIM: RESET", lines), "ADCS_SIM is not reset"),
    (len(grep(r"TO_TEST: BUS_EVENT", w)) == 2, "two BUS_EVENT packets in telemetry"),
], grep(r"BUS DENY|TO_TEST: BUS_EVENT", w))

# ---- B2 ----
w = windows.get(6, [])
scenario("B2", "payload sends telemetry under SAMPLE_APP's ID", [
    (bool(grep(r"BUS DENY sender=payload mid=0x0883 .*reason=sender", w)), "denied: the sender is the ring"),
    (not grep(r"TO_TEST: SAMPLE_APP_HK .*cmd=77 ", lines), "the forged packet is not in telemetry"),
], grep(r"BUS DENY|TO_TEST: BUS_EVENT", w))

# ---- B3 ----
w = windows.get(7, [])
scenario("B3", "pointing request out of range", [
    (len(grep(r"BUS DENY sender=payload mid=0x18D1 cc=1 reason=range", w)) == 1, "the request for 90000 is denied"),
    (bool(grep(r"SET_POINTING roll=1000 pitch=2000 yaw=3000", w)), "the request before it arrives"),
    (bool(grep(r"SET_POINTING roll=4000 pitch=5000 yaw=6000", w)), "the request after it arrives"),
    (max(pointing_values(lines), default=0) <= LIMIT, "ADCS_SIM never sees an angle over the limit"),
], grep(r"BUS DENY|ADCS_SIM: SET_POINTING roll=(1000 pitch=2000|4000 pitch=5000)", w))

# ---- B4 ----
w = windows.get(8, [])
denied_at = next((i for i, l in enumerate(w) if "reason=malformed" in l), len(w))
scenario("B4", "malformed packets", [
    (len(grep(r"BUS DENY sender=payload .*reason=malformed", w)) == 4, "all four kinds are denied as malformed"),
    (len(grep(r"TO_TEST: BUS_HK", w[denied_at:])) >= 2, "the bus keeps running"),
    (bool(grep(r"PAYLOAD_REPORT B4 sent four malformed records", w)), "the payload's next valid record gets through"),
], grep(r"BUS DENY|PAYLOAD_REPORT B4", w))

# ---- B5 ----
w = windows.get(9, [])
gap = max_gap(w)
dropped = [int(m.group(1)) for l in w for m in [re.search(r"reason=rate .*dropped=(\d+)", l)] if m]
t_attack = next((tsc(l) for l in w if "PAYLOAD: ATTACK 9 begins" in l), None)
t_done = next((tsc(l) for l in w if re.search(r"SUPERVISOR: PAYLOAD restart \d+ done", l)), None)
scenario("B5", "flood", [
    (bool(grep(r"BUS DENY sender=payload reason=rate", w)), "the rate limit drops the excess"),
    (bool(grep(r"BUS RESTART_REQUEST sender=payload reason=rate", w)), "the bus asks for a restart"),
    (bool(grep(r"PAYLOAD: started, generation", w)), "a new payload starts"),
    (gap is not None and gap < 1500, "MC telemetry keeps its period"),
], grep(r"BUS DENY", w)[:2] + grep(r"BUS RESTART_REQUEST|SUPERVISOR: (detected|PAYLOAD restart)", w))
results[-1] += (((None, None, ms(t_done - t_attack)) if t_attack and t_done else None, gap, max(dropped, default=0)),)

# ---- B6 ----
w = windows.get(10, [])
hk = [int(m.group(1)) for l in lines for m in [re.search(r"TO_TEST: ADCS_HK .*max_abs=(-?\d+)", l)] if m]
scenario("B6", "payload rewrites a record after publishing it", [
    (bool(grep(r"PAYLOAD: ATTACK 10 begins", w)), "the attack ran"),
    (max(pointing_values(w), default=0) <= LIMIT, "ADCS_SIM never sees an angle over the limit"),
    (bool(hk) and max(hk) <= LIMIT, "its housekeeping shows no angle over the limit"),
], grep(r"BUS DENY|BUS RESTART_REQUEST", w)[:4] + grep(r"TO_TEST: ADCS_HK", w)[-1:])
results[-1] += (len(grep(r"BUS DENY .*reason=range", w)),)

# ---- The end of the run ----
generations = [int(m.group(1)) for l in lines for m in [re.search(r"PAYLOAD: started, generation (\d+)", l)] if m]
last_hk = [int(m.group(1)) for l in after for m in [re.search(r"IMAGER_HK .*generation=(\d+)", l)] if m]
scenario("E", "after the attacks", [
    (bool(grep(r"script done, the word of MC memory is 0x5ec4e7", lines)), "MC's memory is unchanged"),
    (bool(last_hk) and bool(generations) and last_hk[-1] == generations[-1], "the newest payload reports housekeeping"),
    (not grep(r"panic", lines), "no panic"),
], grep(r"script done", lines) + grep(r"TO_TEST: IMAGER_HK", after)[-1:])

# ---- Report ----
print(f"{'ID':4} {'RESULT':6} SCENARIO")
failed = 0
for r in results:
    ident, title, passed, missing = r[0], r[1], r[2], r[3]
    print(f"{ident:4} {'pass' if passed else 'FAIL':6} {title}")
    for what in missing:
        print(f"            not shown: {what}")
    failed += not passed

print("\nEvidence")
for r in results:
    print(f"-- {r[0]}: {r[1]}")
    for line in r[4]:
        print("   " + line.strip()[:200])

print(f"\nMeasurements (timestamp counter at {hz} Hz; emulation times unless run under KVM)")
t_op = next((tsc(l) for l in lines if "cFE is OPERATIONAL" in l), None)
t_pl = next((tsc(l) for l in lines if "PAYLOAD: started, generation 1" in l), None)
if t_op and t_pl:
    print(f"power-on to both containers running: {ms(t_pl):.0f} ms; to cFE OPERATIONAL: {ms(t_op):.0f} ms")
for r in results:
    if r[0] in ("K1a", "K1b") and len(r) > 5 and r[5][0]:
        d, s, done = r[5][0]
        print(f"{r[0]}: attack to detection {d:.0f} ms, to the new payload's first line {s:.0f} ms, "
              f"to restart done {done:.0f} ms; largest telemetry gap {r[5][1]:.0f} ms")
    if r[0] == "U" and len(r) > 5:
        print(f"U: {r[5][0]} page faults of probe threads for {r[5][1]} probed pages")
    if r[0] == "K3" and len(r) > 5:
        print(f"K3: largest telemetry gap {r[5][1]:.0f} ms")
    if r[0] == "B5" and len(r) > 5:
        t, gap, drop = r[5]
        if t:
            print(f"B5: flood start to restart done {t[2]:.0f} ms; {drop} records dropped; "
                  f"largest telemetry gap {gap:.0f} ms")
    if r[0] == "B6" and len(r) > 5:
        print(f"B6: {r[5]} rewritten records were read with a forbidden angle and denied")
kills = [int(m.group(1)) for l in lines for m in [re.search(r"SUPERVISOR: killed PAYLOAD: (\d+) us", l)] if m]
starts_us = [int(m.group(1)) for l in lines
             for m in [re.search(r"SUPERVISOR: started PAYLOAD .* (\d+) us, tsc", l)] if m]
restarts = [int(m.group(1)) for l in lines
            for m in [re.search(r"SUPERVISOR: PAYLOAD restart \d+ done in (\d+) us", l)] if m]
if kills and restarts:
    print(f"payload restarts: {len(restarts)}; kill_container {min(kills)} to {max(kills)} us; "
          f"load and start {min(starts_us)} to {max(starts_us)} us; whole restart {min(restarts)} to {max(restarts)} us")
gap_all = max_gap(lines[starts[0][0]:end]) if starts else None
if gap_all is not None:
    print(f"largest gap between SAMPLE_APP housekeeping packets over all attacks: {gap_all:.0f} ms (period 1000 ms)")
cycles = grep(r"BUS CYCLES", lines)
if cycles:
    print("bus, cycles per record: " + cycles[-1].split("BUS CYCLES ")[1])
stats = grep(r"BUS STATS", lines)
if stats:
    print("bus, last counters: " + stats[-1].split("BUS STATS ")[1])

print(f"\n{len(results) - failed} of {len(results)} scenarios pass")
sys.exit(1 if failed else 0)
