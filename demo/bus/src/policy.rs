//! The policy of the bus: who may send what, and where it goes.
//!
//! This file is the whole policy. It is an allow-list: a record that no row
//! allows is denied. The table is written by hand here; a policy compiler
//! can replace this file without touching the rest of the bus.

/// A process, as the bus knows it: by the ring a record arrives on.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Proc {
    Mc = 0,
    #[allow(dead_code)]
    Bus = 1,
    Payload = 2,
}

/// Any function code. Telemetry has none.
pub const ANY_CC: u8 = 0xff;

/// A limit on one little-endian signed 32-bit field of the payload.
pub struct Field {
    /// Offset from the start of the packet.
    pub offset: usize,
    pub min: i32,
    pub max: i32,
}

pub struct Allow {
    pub sender: Proc,
    pub msg_id: u16,
    pub fcn_code: u8,
    /// The exact length of the packet, headers included.
    pub length: usize,
    pub fields: &'static [Field],
}

pub struct Route {
    pub msg_id: u16,
    pub to: Proc,
}

// Message IDs and layouts: demo/include/demo_msg.h.
pub const PAYLOAD_CMD_MID: u16 = 0x18D0;
pub const ADCS_CMD_MID: u16 = 0x18D1;
pub const SUPERVISOR_CMD_MID: u16 = 0x18D2;
pub const IMAGER_HK_TLM_MID: u16 = 0x08D0;
pub const PAYLOAD_REPORT_MID: u16 = 0x08D1;
pub const BUS_EVENT_TLM_MID: u16 = 0x08D2;
pub const BUS_HK_TLM_MID: u16 = 0x08D4;

const CMD_HDR: usize = 8;
const TLM_HDR: usize = 16;
const MAX_MILLIDEG: i32 = 30_000;

const POINTING: [Field; 3] = [
    Field { offset: CMD_HDR, min: -MAX_MILLIDEG, max: MAX_MILLIDEG },
    Field { offset: CMD_HDR + 4, min: -MAX_MILLIDEG, max: MAX_MILLIDEG },
    Field { offset: CMD_HDR + 8, min: -MAX_MILLIDEG, max: MAX_MILLIDEG },
];

pub const ALLOW: &[Allow] = &[
    // MC commands the payload: no-op and the demo's attack selector.
    Allow { sender: Proc::Mc, msg_id: PAYLOAD_CMD_MID, fcn_code: 0, length: CMD_HDR, fields: &[] },
    Allow { sender: Proc::Mc, msg_id: PAYLOAD_CMD_MID, fcn_code: 1, length: CMD_HDR + 16, fields: &[] },
    // The payload reports its housekeeping and one line of text.
    Allow { sender: Proc::Payload, msg_id: IMAGER_HK_TLM_MID, fcn_code: ANY_CC, length: TLM_HDR + 16, fields: &[] },
    Allow { sender: Proc::Payload, msg_id: PAYLOAD_REPORT_MID, fcn_code: ANY_CC, length: TLM_HDR + 96, fields: &[] },
    // The payload may ask attitude control for a pointing within limits,
    // and for nothing else.
    Allow { sender: Proc::Payload, msg_id: ADCS_CMD_MID, fcn_code: 1, length: CMD_HDR + 12, fields: &POINTING },
];

pub const ROUTE: &[Route] = &[
    Route { msg_id: PAYLOAD_CMD_MID, to: Proc::Payload },
    Route { msg_id: IMAGER_HK_TLM_MID, to: Proc::Mc },
    Route { msg_id: PAYLOAD_REPORT_MID, to: Proc::Mc },
    Route { msg_id: ADCS_CMD_MID, to: Proc::Mc },
];

/// Records that one sender may send in one window of 100 ms.
pub const RATE_PER_WINDOW: u32 = 20;
pub const RATE_WINDOW_NS: u64 = 100_000_000;
/// Denials within one second after which the bus asks for a restart of the
/// sender, and the time over its rate after which it does the same.
pub const DENIALS_PER_SECOND: u32 = 3;
pub const OVER_RATE_NS: u64 = 1_000_000_000;
