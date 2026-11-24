//! The message bus of the attack demo.
//!
//! The bus is the only process that maps the rings of more than one other
//! process. It reads records from each sender's ring, checks each against
//! the policy in `policy.rs`, and forwards it to the ring of its destination
//! or drops it.
//!
//! The bus starts with the rings of the mission-critical process (MC) only.
//! MC sends it the ring pages of each new payload over an endpoint, and the
//! bus maps them where the layout says. MC itself then unmaps them.
//!
//! Three rules shape the code:
//!
//! - The sender of a record is the ring it came from. No field of a packet
//!   says who sent it.
//! - A record is copied into private memory once, before anything looks at
//!   it. All checks and the forwarding use that copy, so a sender that
//!   rewrites its ring after the check changes nothing.
//! - Nothing a sender writes can stop the loop: counts and lengths from a
//!   ring are bounds-checked, each sender has a rate limit, and a sender that
//!   keeps violating the policy is cut off and reported.

#![no_std]
#![no_main]

mod policy;
mod ring;

use core::fmt::{self, Write};
use core::panic::PanicInfo;

use policy::{Proc, ALLOW, ANY_CC, ROUTE};
use ring::{Got, Ring, MAX_REC};

// demo/include/demo_layout.h
const PAGE: usize = 4096;
const BASE: usize = 0xb0_0000_0000;
const MC_OUT: usize = BASE + 272 * PAGE;
const MC_IN: usize = BASE + 274 * PAGE;
const PL_OUT: usize = BASE + 276 * PAGE;
const PL_IN: usize = BASE + 278 * PAGE;

const CMD_HDR: usize = 8;
const TLM_HDR: usize = 16;

/// Records taken from one ring before the bus looks at the next one.
const BATCH: usize = 8;

/// The pages of the payload's two rings.
const PL_RING_PAGES: usize = 4;
/// The descriptor of the endpoint that MC sends those pages on.
const MC_ENDPOINT: usize = 0;

// Words in the headers of MC's rings; see demo/mc/demo_mc.c.
const OUT_GENERATION: usize = 0;
const OUT_REMAP: usize = 1;
const IN_PL_BEAT: usize = 1;
const IN_REMAP_ACK: usize = 2;
const IN_REMAP_DONE: usize = 3;

#[unsafe(naked)]
#[no_mangle]
pub unsafe extern "C" fn _start() -> ! {
    core::arch::naked_asm!("and rsp, -16", "xor ebp, ebp", "call {main}", "ud2", main = sym main)
}

// ---- Console and clock ----

struct Console {
    line: [u8; 200],
    len: usize,
}

impl Console {
    fn flush(&mut self) {
        if self.len != 0 {
            unsafe { vsys::sys_debug_print(self.line.as_ptr(), self.len) };
            self.len = 0;
        }
    }
}

impl Write for Console {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for &b in s.as_bytes() {
            if b == b'\n' {
                self.flush();
            } else if self.len < self.line.len() {
                self.line[self.len] = b;
                self.len += 1;
            }
        }
        Ok(())
    }
}

fn rdtsc() -> u64 {
    unsafe { core::arch::x86_64::_rdtsc() }
}

struct Clock {
    hz: u64,
}

impl Clock {
    fn ns(&self, ticks: u64) -> u64 {
        ticks / self.hz * 1_000_000_000 + ticks % self.hz * 1_000_000_000 / self.hz
    }
}

// ---- Why a record is denied ----

#[derive(Clone, Copy, PartialEq, Eq)]
enum Deny {
    Malformed = 1,
    Sender = 2,
    Command = 3,
    Range = 4,
    Rate = 5,
    NoRoute = 6,
}

impl Deny {
    fn name(self) -> &'static str {
        match self {
            Deny::Malformed => "malformed",
            Deny::Sender => "sender",
            Deny::Command => "command",
            Deny::Range => "range",
            Deny::Rate => "rate",
            Deny::NoRoute => "noroute",
        }
    }
}

fn proc_name(p: Proc) -> &'static str {
    match p {
        Proc::Mc => "mc",
        Proc::Bus => "bus",
        Proc::Payload => "payload",
    }
}

/// What the bus reads out of a packet. It is built from the private copy.
struct Parsed {
    msg_id: u16,
    /// The function code of a command; `ANY_CC` for telemetry.
    fcn_code: u8,
}

/// Checks the headers of the packet in `pkt` against its length.
fn parse(pkt: &[u8]) -> Result<Parsed, Deny> {
    if pkt.len() < 6 {
        return Err(Deny::Malformed);
    }
    let version = pkt[0] >> 5;
    let is_cmd = pkt[0] & 0x10 != 0;
    let has_secondary = pkt[0] & 0x08 != 0;
    let length = ((pkt[4] as usize) << 8 | pkt[5] as usize) + 7;
    if version != 0 || !has_secondary || length != pkt.len() {
        return Err(Deny::Malformed);
    }
    if pkt.len() < if is_cmd { CMD_HDR } else { TLM_HDR } {
        return Err(Deny::Malformed);
    }
    let msg_id = (pkt[0] as u16) << 8 | pkt[1] as u16;
    Ok(Parsed { msg_id, fcn_code: if is_cmd { pkt[6] & 0x7f } else { ANY_CC } })
}

/// Checks a parsed packet from `sender` against the allow-list and finds its
/// destination.
fn check(sender: Proc, p: &Parsed, pkt: &[u8]) -> Result<Proc, Deny> {
    let mut reason = Deny::Sender;
    let mut allowed = false;
    for row in ALLOW {
        if row.sender != sender || row.msg_id != p.msg_id {
            continue;
        }
        if row.fcn_code != ANY_CC && row.fcn_code != p.fcn_code {
            reason = Deny::Command;
            continue;
        }
        if row.length != pkt.len() {
            return Err(Deny::Malformed);
        }
        for f in row.fields {
            let b = &pkt[f.offset..f.offset + 4];
            let value = i32::from_le_bytes([b[0], b[1], b[2], b[3]]);
            if value < f.min || value > f.max {
                return Err(Deny::Range);
            }
        }
        allowed = true;
        break;
    }
    if !allowed {
        return Err(reason);
    }
    for route in ROUTE {
        if route.msg_id == p.msg_id {
            return Ok(route.to);
        }
    }
    Err(Deny::NoRoute)
}

// ---- Per-sender state ----

struct Sender {
    forwarded: u32,
    denied: u32,
    dropped: u32,
    window_start: u64,
    window_count: u32,
    window_dropped: u32,
    /// Since when every window had drops; zero if the last one had none.
    over_since: u64,
    /// When the last few denials happened.
    recent: [u64; policy::DENIALS_PER_SECOND as usize],
    /// The bus has asked for a restart of this sender and ignores it.
    cut_off: bool,
}

impl Sender {
    const fn new() -> Self {
        Self {
            forwarded: 0,
            denied: 0,
            dropped: 0,
            window_start: 0,
            window_count: 0,
            window_dropped: 0,
            over_since: 0,
            recent: [0; policy::DENIALS_PER_SECOND as usize],
            cut_off: false,
        }
    }
}

/// Cycle counts of handled records, for the report.
struct Samples {
    v: [u32; 2048],
    n: usize,
}

impl Samples {
    fn add(&mut self, cycles: u64) {
        if self.n < self.v.len() {
            self.v[self.n] = cycles.min(u32::MAX as u64) as u32;
            self.n += 1;
        }
    }

    /// Sorts the samples and returns the median, the 99th percentile, and
    /// the largest.
    fn summary(&mut self) -> (u32, u32, u32) {
        let v = &mut self.v[..self.n];
        if v.is_empty() {
            return (0, 0, 0);
        }
        v.sort_unstable();
        (v[v.len() / 2], v[(v.len() * 99 / 100).min(v.len() - 1)], v[v.len() - 1])
    }
}

struct Bus {
    out: Console,
    clock: Clock,
    mc_out: Ring,
    mc_in: Ring,
    pl_out: Ring,
    pl_in: Ring,
    mc: Sender,
    payload: Sender,
    /// The payload generation that MC wrote last; odd while there is no
    /// payload to serve.
    generation: u64,
    /// The remap request of MC that the bus handled last.
    remap: u64,
    /// The payload's rings are mapped.
    pl_mapped: bool,
    seq: u16,
    restart_requests: u32,
    forward_cycles: Samples,
    deny_cycles: Samples,
}

fn checksum(pkt: &mut [u8]) {
    pkt[7] = 0;
    let mut sum = 0xffu8;
    for b in pkt.iter() {
        sum ^= *b;
    }
    pkt[7] = sum;
}

impl Bus {
    fn header(&mut self, pkt: &mut [u8], msg_id: u16) {
        let len = pkt.len() - 7;
        pkt[0] = (msg_id >> 8) as u8;
        pkt[1] = msg_id as u8;
        pkt[2] = 0xc0 | ((self.seq >> 8) as u8 & 0x3f);
        pkt[3] = self.seq as u8;
        pkt[4] = (len >> 8) as u8;
        pkt[5] = len as u8;
        self.seq = self.seq.wrapping_add(1);
    }

    /// Tells MC about a denied record.
    fn event(&mut self, who: Proc, reason: Deny, msg_id: u16, fcn_code: u8, denials: u32) {
        let mut pkt = [0u8; TLM_HDR + 20];
        self.header(&mut pkt, policy::BUS_EVENT_TLM_MID);
        let words = [who as u32, reason as u32, msg_id as u32, fcn_code as u32, denials];
        for (i, w) in words.iter().enumerate() {
            pkt[TLM_HDR + 4 * i..TLM_HDR + 4 * i + 4].copy_from_slice(&w.to_le_bytes());
        }
        self.mc_in.put(&pkt);
    }

    /// Asks MC to restart a sender, and ignores the sender from now on.
    fn request_restart(&mut self, who: Proc, reason: Deny, now: u64) {
        let (denied, dropped) = (self.payload.denied, self.payload.dropped);
        let mut pkt = [0u8; CMD_HDR + 16];
        self.header(&mut pkt, policy::SUPERVISOR_CMD_MID);
        pkt[6] = 1; // RESTART_REQUEST
        let words = [who as u32, reason as u32, denied, dropped];
        for (i, w) in words.iter().enumerate() {
            pkt[CMD_HDR + 4 * i..CMD_HDR + 4 * i + 4].copy_from_slice(&w.to_le_bytes());
        }
        checksum(&mut pkt);
        self.mc_in.put(&pkt);
        self.restart_requests += 1;
        let _ = writeln!(
            self.out,
            "BUS RESTART_REQUEST sender={} reason={} denied={} dropped={} t={}us",
            proc_name(who),
            reason.name(),
            denied,
            dropped,
            self.clock.ns(now) / 1000
        );
    }

    /// Records a denial. Returns true if the sender has now passed the
    /// threshold.
    fn deny(&mut self, who: Proc, reason: Deny, msg_id: u16, fcn_code: u8, now: u64) -> bool {
        let second = self.clock.hz;
        let s = if who == Proc::Mc { &mut self.mc } else { &mut self.payload };
        s.denied += 1;
        let denials = s.denied;
        // Over the threshold: the last few denials, this one included, all
        // happened within one second.
        s.recent.rotate_left(1);
        *s.recent.last_mut().unwrap() = now;
        let oldest = s.recent[0];
        let over = oldest != 0 && now - oldest < second;
        let _ = writeln!(
            self.out,
            "BUS DENY sender={} mid=0x{:04X} cc={} reason={} denials={}",
            proc_name(who),
            msg_id,
            fcn_code,
            reason.name(),
            denials
        );
        self.event(who, reason, msg_id, fcn_code, denials);
        over
    }

    /// Counts a record against the sender's rate. Returns false if the
    /// record is over the rate and must be dropped.
    fn within_rate(&mut self, who: Proc, now: u64) -> bool {
        let window = self.clock.hz / 1_000_000_000 * policy::RATE_WINDOW_NS
            + self.clock.hz % 1_000_000_000 * policy::RATE_WINDOW_NS / 1_000_000_000;
        let s = if who == Proc::Mc { &mut self.mc } else { &mut self.payload };
        if now - s.window_start >= window {
            if s.window_dropped == 0 {
                s.over_since = 0;
            }
            s.window_start = now;
            s.window_count = 0;
            s.window_dropped = 0;
        }
        s.window_count += 1;
        if s.window_count <= policy::RATE_PER_WINDOW {
            return true;
        }
        s.dropped += 1;
        s.window_dropped += 1;
        if s.over_since == 0 {
            s.over_since = now;
        }
        false
    }

    /// Handles up to `BATCH` records of one sender.
    fn serve(&mut self, who: Proc) {
        let mut buf = [0u8; MAX_REC];
        for _ in 0..BATCH {
            let start = rdtsc();
            let got = if who == Proc::Mc { self.mc_out.get(&mut buf) } else { self.pl_out.get(&mut buf) };
            let cut_off = if who == Proc::Mc { false } else { self.payload.cut_off };
            let len = match got {
                Got::Empty => return,
                Got::Record(_) if cut_off => continue,
                Got::Bad if cut_off => continue,
                Got::Bad => {
                    if self.deny(who, Deny::Malformed, 0, 0, start) && who != Proc::Mc {
                        self.payload.cut_off = true;
                        self.request_restart(who, Deny::Malformed, start);
                    }
                    continue;
                }
                Got::Record(len) => len,
            };
            // From here on only the private copy is used.
            let pkt = &buf[..len];
            if !self.within_rate(who, start) {
                let s = if who == Proc::Mc { &self.mc } else { &self.payload };
                let (first, dropped, over_since) = (s.window_dropped == 1, s.dropped, s.over_since);
                if first {
                    let _ = writeln!(self.out, "BUS DENY sender={} reason=rate dropped={}", proc_name(who), dropped);
                    self.event(who, Deny::Rate, 0, 0, dropped);
                }
                let over = over_since != 0 && self.clock.ns(start - over_since) >= policy::OVER_RATE_NS;
                if over && who != Proc::Mc {
                    self.payload.cut_off = true;
                    self.request_restart(who, Deny::Rate, start);
                }
                continue;
            }
            let verdict = parse(pkt).and_then(|p| check(who, &p, pkt).map(|to| (p, to)));
            match verdict {
                Ok((_, to)) => {
                    let sent = match to {
                        Proc::Mc => self.mc_in.put(pkt),
                        Proc::Payload => self.pl_in.put(pkt),
                        Proc::Bus => false,
                    };
                    let s = if who == Proc::Mc { &mut self.mc } else { &mut self.payload };
                    if sent {
                        s.forwarded += 1;
                        self.forward_cycles.add(rdtsc() - start);
                    } else {
                        // The destination does not take its messages.
                        s.dropped += 1;
                    }
                }
                Err(reason) => {
                    // The cost of the decision, without the log line.
                    self.deny_cycles.add(rdtsc() - start);
                    let (msg_id, fcn_code) = match parse(pkt) {
                        Ok(p) => (p.msg_id, if p.fcn_code == ANY_CC { 0 } else { p.fcn_code }),
                        Err(_) => (0, 0),
                    };
                    let over = self.deny(who, reason, msg_id, fcn_code, start);
                    if over && who != Proc::Mc {
                        self.payload.cut_off = true;
                        self.request_restart(who, reason, start);
                    }
                }
            }
        }
    }

    /// Replaces the rings of the previous payload with those of the new one,
    /// which MC sends on the endpoint.
    fn take_rings(&mut self, remap: u64) {
        if self.pl_mapped {
            let ret = unsafe { vsys::sys_unmap_4k(PL_OUT, PL_RING_PAGES) };
            if ret < 0 {
                let _ = writeln!(self.out, "BUS unmap of the old payload rings: {}", vsys::error_name(ret));
            }
            self.pl_mapped = false;
        }
        // MC waits for this word before it sends, so its send finds the bus
        // waiting. The receive blocks until MC sends.
        self.mc_in.set_arg(IN_REMAP_ACK, remap);
        let ret = unsafe { vsys::sys_receive_pages(MC_ENDPOINT, PL_OUT, PL_RING_PAGES) };
        if ret < 0 {
            let _ = writeln!(self.out, "BUS receive of the payload rings: {}", vsys::error_name(ret));
            return;
        }
        self.pl_mapped = true;
        self.mc_in.set_arg(IN_REMAP_DONE, remap);
    }

    /// Sends the counters to MC.
    fn housekeeping(&mut self) {
        let mut pkt = [0u8; TLM_HDR + 44];
        self.header(&mut pkt, policy::BUS_HK_TLM_MID);
        let words = [
            0,
            self.mc.forwarded,
            0,
            self.payload.forwarded,
            self.mc.denied,
            0,
            self.payload.denied,
            self.mc.dropped,
            0,
            self.payload.dropped,
            self.restart_requests,
        ];
        for (i, w) in words.iter().enumerate() {
            pkt[TLM_HDR + 4 * i..TLM_HDR + 4 * i + 4].copy_from_slice(&w.to_le_bytes());
        }
        self.mc_in.put(&pkt);
    }

    fn report(&mut self) {
        let (fm, fp, fx) = self.forward_cycles.summary();
        let (dm, dp, dx) = self.deny_cycles.summary();
        let _ = writeln!(
            self.out,
            "BUS STATS forwarded mc={} payload={} denied mc={} payload={} dropped mc={} payload={} restarts={}",
            self.mc.forwarded,
            self.payload.forwarded,
            self.mc.denied,
            self.payload.denied,
            self.mc.dropped,
            self.payload.dropped,
            self.restart_requests
        );
        let _ = writeln!(
            self.out,
            "BUS CYCLES forward n={} median={} p99={} max={} deny n={} median={} p99={} max={}",
            self.forward_cycles.n, fm, fp, fx, self.deny_cycles.n, dm, dp, dx
        );
    }

    fn run(&mut self) -> ! {
        let second = self.clock.hz;
        let mut next_hk = rdtsc() + second;
        let mut next_report = rdtsc() + 5 * second;
        loop {
            self.mc_in.beat();
            self.serve(Proc::Mc);

            // MC is about to send the rings of a new payload.
            let remap = self.mc_out.arg(OUT_REMAP);
            if remap != self.remap {
                self.remap = remap;
                self.take_rings(remap);
            }

            // MC writes an odd generation while there is no payload to
            // serve and an even one when the new payload's rings are here.
            // The word is in MC's ring, which the payload does not map.
            let generation = self.mc_out.arg(OUT_GENERATION);
            if generation != self.generation && generation % 2 == 0 && self.pl_mapped {
                self.generation = generation;
                self.pl_out.reset();
                self.pl_in.reset();
                // A new payload starts with a clean record. The counters of
                // what the bus forwarded, denied, and dropped keep running.
                let old = core::mem::replace(&mut self.payload, Sender::new());
                self.payload.forwarded = old.forwarded;
                self.payload.denied = old.denied;
                self.payload.dropped = old.dropped;
                let _ = writeln!(self.out, "BUS payload generation {}", generation / 2);
            }
            if generation == self.generation && self.pl_mapped {
                self.serve(Proc::Payload);
                // MC does not map the payload's rings; pass its heartbeat on.
                self.mc_in.set_arg(IN_PL_BEAT, self.pl_out.heartbeat());
            }

            let now = rdtsc();
            if now >= next_hk {
                next_hk = now + second;
                self.housekeeping();
            }
            if now >= next_report {
                next_report = now + 5 * second;
                self.report();
            }
            core::hint::spin_loop();
        }
    }
}

extern "C" fn main() -> ! {
    let (hz, _source) = unsafe { vsys::sys_debug_tsc_hz() };
    let mut bus = Bus {
        out: Console { line: [0; 200], len: 0 },
        clock: Clock { hz: if hz >= 1_000_000 { hz } else { 1_000_000_000 } },
        mc_out: Ring::new(MC_OUT),
        mc_in: Ring::new(MC_IN),
        pl_out: Ring::new(PL_OUT),
        pl_in: Ring::new(PL_IN),
        mc: Sender::new(),
        payload: Sender::new(),
        generation: u64::MAX,
        remap: 0,
        pl_mapped: false,
        seq: 0,
        restart_requests: 0,
        forward_cycles: Samples { v: [0; 2048], n: 0 },
        deny_cycles: Samples { v: [0; 2048], n: 0 },
    };
    // The page tables under the payload's rings are paid from the bus's quota.
    let quota = unsafe { vsys::sys_alloc_thread_quota_4k(64) };
    let _ = writeln!(
        bus.out,
        "BUS started, {} allow rows, {} routes, quota: {}",
        ALLOW.len(),
        ROUTE.len(),
        vsys::error_name(quota)
    );
    bus.run()
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    let mut out = Console { line: [0; 200], len: 0 };
    let _ = writeln!(out, "BUS panic: {}", info);
    loop {
        unsafe { vsys::syscall0(vsys::NR_THREAD_EXIT) };
    }
}
