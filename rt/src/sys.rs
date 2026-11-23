//! The kernel interface of the runtime.
//!
//! This is the only module that makes system calls. The calls are the ones of
//! VeriFlat, the concurrent Atmosphere kernel, through its `vsys` stubs.

use core::cell::UnsafeCell;

/// The size of a page.
pub const PAGE_SIZE: usize = 4096;

/// The kernel maps at most this many pages in one call here, so that the
/// quota the runtime holds for a call stays small.
const MAP_CHUNK: usize = 512;

/// Writes one line to the kernel console. The kernel adds the line break.
pub fn console_line(line: &[u8]) {
    unsafe {
        vsys::sys_debug_print(line.as_ptr(), line.len());
    }
}

/// A lower bound of the 4K quota that this thread holds.
///
/// A thread pays for the pages it maps, and for the page-table pages under
/// them, from its own quota. Quota moves from the container to the process
/// and from the process to the thread. The kernel has no call that reads the
/// quota, so the runtime keeps count of what it moved and of the most that
/// each mapping can have used.
struct Quota(UnsafeCell<usize>);
unsafe impl Sync for Quota {}

static QUOTA: Quota = Quota(UnsafeCell::new(0));

/// Makes the thread hold at least `amount` 4K quota.
fn fund(amount: usize) -> bool {
    let held = unsafe { &mut *QUOTA.0.get() };
    if *held >= amount {
        return true;
    }
    let more = amount - *held;
    unsafe {
        if vsys::sys_alloc_quota_4k(more) < 0 {
            return false;
        }
        if vsys::sys_alloc_thread_quota_4k(more) < 0 {
            return false;
        }
    }
    *held += more;
    true
}

/// The number of aligned blocks of `1 << shift` bytes that the range touches.
fn windows(va: usize, pages: usize, shift: u32) -> usize {
    let last = va + pages * PAGE_SIZE - 1;
    (last >> shift) - (va >> shift) + 1
}

/// Maps `pages` fresh, zeroed, writable pages at `va`. Returns `false` if the
/// kernel refuses. A refusal can leave the first part of the range mapped.
pub fn map_pages(va: usize, pages: usize) -> bool {
    let mut va = va;
    let mut left = pages;
    while left != 0 {
        let n = left.min(MAP_CHUNK);
        // The kernel asks for four times the range before it starts.
        if !fund(4 * n) {
            return false;
        }
        if unsafe { vsys::sys_mmap_4k(va, n) } < 0 {
            return false;
        }
        // One quota per page, and at most one per page table, directory, and
        // directory pointer table that the range touches.
        let used = n + windows(va, n, 21) + windows(va, n, 30) + windows(va, n, 39);
        let held = unsafe { &mut *QUOTA.0.get() };
        *held = held.saturating_sub(used);
        va += n * PAGE_SIZE;
        left -= n;
    }
    true
}

/// Reads the timestamp counter.
pub fn rdtsc() -> u64 {
    unsafe { core::arch::x86_64::_rdtsc() }
}

/// Where the rate of the timestamp counter comes from.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum RateSource {
    /// The kernel reported it.
    Kernel,
    /// A CPUID leaf reported it.
    Cpuid,
    /// Nothing reported it. The rate is a guess.
    Assumed,
}

/// The rate of the timestamp counter in Hz.
///
/// A process has no I/O privilege, so it cannot measure the counter against
/// a timer itself. The kernel reports the rate where it has the call for it.
/// Otherwise the processor may: leaf 15H gives the exact ratio to the crystal
/// clock, leaf 16H the base frequency, and a hypervisor gives the rate in
/// leaf 4000_0010H.
pub fn counter_hz() -> (u64, RateSource) {
    // The upstream kernel does not have the entry yet and answers with an
    // error. A kernel that only assumes a rate says so in the source.
    let reported = unsafe { vsys::syscall0(vsys::NR_DEBUG_TSC_HZ) };
    if reported.rax > 0 && reported.rdx != 0 {
        return (reported.rax as u64, RateSource::Kernel);
    }
    use core::arch::x86_64::__cpuid;
    let max = __cpuid(0).eax;
    if max >= 0x15 {
        let leaf = __cpuid(0x15);
        if leaf.eax != 0 && leaf.ebx != 0 && leaf.ecx != 0 {
            return (leaf.ecx as u64 * leaf.ebx as u64 / leaf.eax as u64, RateSource::Cpuid);
        }
    }
    if max >= 0x16 {
        let mhz = __cpuid(0x16).eax & 0xffff;
        if mhz != 0 {
            return (mhz as u64 * 1_000_000, RateSource::Cpuid);
        }
    }
    // Leaf 1, ECX bit 31: a hypervisor is present and has leaves from 4000_0000H.
    if __cpuid(1).ecx & (1 << 31) != 0 && __cpuid(0x4000_0000).eax >= 0x4000_0010 {
        let khz = __cpuid(0x4000_0010).eax;
        if khz != 0 {
            return (khz as u64 * 1_000, RateSource::Cpuid);
        }
    }
    if reported.rax > 0 {
        return (reported.rax as u64, RateSource::Assumed);
    }
    (1_000_000_000, RateSource::Assumed)
}

/// Ends the program.
///
/// The kernel shuts the machine down. Under QEMU, the emulator ends with the
/// status `code << 1 | 1`.
pub fn exit(code: u32) -> ! {
    unsafe { vsys::sys_debug_exit(code as usize) }
}
