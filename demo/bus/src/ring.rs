//! The shared ring of `demo/include/demo_ring.h`, from the bus's side.
//!
//! A ring page is writable by the process on the other end, so nothing in it
//! is trusted. The bus keeps its own count for each ring and reads every
//! shared word exactly once per decision.

use core::ptr::{read_volatile, write_volatile};

pub const DATA: u32 = 4096;
const MASK: u32 = DATA - 1;
pub const MAX_REC: usize = 1024;

const TAIL: usize = 0;
const HEAD: usize = 64;
const HEARTBEAT: usize = 128;
const ARG: usize = 144;
const DATA_AT: usize = 4096;

/// What a read of a ring found.
pub enum Got {
    Empty,
    /// A record of this many bytes is in the caller's buffer.
    Record(usize),
    /// The counts or the length made no sense. The ring was emptied.
    Bad,
}

pub struct Ring {
    base: *mut u8,
    /// The count that the bus owns on this ring.
    mine: u32,
}

impl Ring {
    pub const fn new(base: usize) -> Self {
        Self { base: base as *mut u8, mine: 0 }
    }

    fn word(&self, at: usize) -> u32 {
        unsafe { read_volatile(self.base.add(at) as *const u32) }
    }

    fn set_word(&self, at: usize, value: u32) {
        unsafe { write_volatile(self.base.add(at) as *mut u32, value) }
    }

    /// Forgets the bus's count. The owner of the ring has zeroed it.
    pub fn reset(&mut self) {
        self.mine = 0;
    }

    pub fn arg(&self, i: usize) -> u64 {
        unsafe { read_volatile(self.base.add(ARG + 8 * i) as *const u64) }
    }

    pub fn set_arg(&self, i: usize, value: u64) {
        unsafe { write_volatile(self.base.add(ARG + 8 * i) as *mut u64, value) }
    }

    pub fn heartbeat(&self) -> u64 {
        unsafe { read_volatile(self.base.add(HEARTBEAT) as *const u64) }
    }

    pub fn beat(&self) {
        unsafe {
            let p = self.base.add(HEARTBEAT) as *mut u64;
            write_volatile(p, read_volatile(p).wrapping_add(1));
        }
    }

    /// Copies the next record into `buf`, which is private to the bus.
    pub fn get(&mut self, buf: &mut [u8; MAX_REC]) -> Got {
        let tail = self.word(TAIL);
        let avail = tail.wrapping_sub(self.mine);
        if avail == 0 {
            return Got::Empty;
        }
        if avail > DATA || avail < 2 {
            return self.bad(tail);
        }
        let len = self.byte(self.mine) as usize | (self.byte(self.mine.wrapping_add(1)) as usize) << 8;
        if len == 0 || len > MAX_REC || len + 2 > avail as usize {
            return self.bad(tail);
        }
        for (i, b) in buf[..len].iter_mut().enumerate() {
            *b = self.byte(self.mine.wrapping_add(2 + i as u32));
        }
        self.mine = self.mine.wrapping_add(len as u32 + 2);
        self.set_word(HEAD, self.mine);
        Got::Record(len)
    }

    fn bad(&mut self, tail: u32) -> Got {
        self.mine = tail;
        self.set_word(HEAD, tail);
        Got::Bad
    }

    /// Appends a record. Returns false if it does not fit now.
    pub fn put(&mut self, rec: &[u8]) -> bool {
        let used = self.mine.wrapping_sub(self.word(HEAD));
        if rec.len() > MAX_REC || used > DATA || ((DATA - used) as usize) < rec.len() + 2 {
            return false;
        }
        self.set_byte(self.mine, rec.len() as u8);
        self.set_byte(self.mine.wrapping_add(1), (rec.len() >> 8) as u8);
        for (i, b) in rec.iter().enumerate() {
            self.set_byte(self.mine.wrapping_add(2 + i as u32), *b);
        }
        self.mine = self.mine.wrapping_add(rec.len() as u32 + 2);
        self.set_word(TAIL, self.mine);
        true
    }

    fn byte(&self, count: u32) -> u8 {
        unsafe { read_volatile(self.base.add(DATA_AT + (count & MASK) as usize)) }
    }

    fn set_byte(&self, count: u32, value: u8) {
        unsafe { write_volatile(self.base.add(DATA_AT + (count & MASK) as usize), value) }
    }
}
