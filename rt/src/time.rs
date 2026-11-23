//! The monotonic clock.
//!
//! Time is the timestamp counter scaled to nanoseconds. The rate of the
//! counter comes from the kernel or from the processor; see
//! [`sys::counter_hz`].

use core::cell::UnsafeCell;

use crate::sys::{self, RateSource};

/// A deadline that never arrives.
pub const FOREVER: u64 = u64::MAX;

struct Clock {
    /// The counter value at start.
    base: u64,
    /// Nanoseconds per counter tick as a 32.32 fixed-point number.
    scale: u64,
    /// The counter frequency in Hz.
    hz: u64,
    /// False if nothing reported the frequency.
    known: bool,
}

struct Shared(UnsafeCell<Clock>);
unsafe impl Sync for Shared {}

// Until `init` runs the clock assumes 1 GHz, so that an early reading is sane.
static CLOCK: Shared =
    Shared(UnsafeCell::new(Clock { base: 0, scale: 1 << 32, hz: 1_000_000_000, known: false }));

/// Reads the counter frequency and starts the clock at zero.
pub fn init() {
    let (hz, source) = sys::counter_hz();
    let clock = unsafe { &mut *CLOCK.0.get() };
    if hz >= 1_000_000 {
        clock.hz = hz;
        clock.scale = ((1_000_000_000u128 << 32) / hz as u128) as u64;
        clock.known = source != RateSource::Assumed;
    }
    clock.base = sys::rdtsc();
}

/// Nanoseconds since `init`.
pub fn now() -> u64 {
    let clock = unsafe { &*CLOCK.0.get() };
    let ticks = sys::rdtsc().wrapping_sub(clock.base);
    ((ticks as u128 * clock.scale as u128) >> 32) as u64
}

/// The frequency of the timestamp counter in Hz.
pub fn counter_hz() -> u64 {
    unsafe { (*CLOCK.0.get()).hz }
}

/// Nanoseconds since the runtime started. Never goes backward.
#[no_mangle]
pub extern "C" fn art_clock_ns() -> u64 {
    now()
}

/// The frequency of the timestamp counter in Hz.
#[no_mangle]
pub extern "C" fn art_clock_counter_hz() -> u64 {
    counter_hz()
}

/// Nonzero if the kernel or the processor reported the counter frequency.
/// Zero if the runtime had to assume one; time is then not in real units.
#[no_mangle]
pub extern "C" fn art_clock_calibrated() -> i32 {
    unsafe { (*CLOCK.0.get()).known as i32 }
}
