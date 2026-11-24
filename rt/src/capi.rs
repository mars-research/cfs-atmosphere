//! The C interface of the task calls. See `include/atmo_rt.h`.

use core::ffi::c_void;

use crate::sync::{ERROR, NO_MEMORY, OK};
use crate::task::{self, Entry, Error};
use crate::time;

fn code(result: Result<(), Error>) -> i32 {
    match result {
        Ok(()) => OK,
        Err(Error::NoMemory) => NO_MEMORY,
        Err(_) => ERROR,
    }
}

/// Creates a task. With a null `stack` the runtime maps the stack.
#[no_mangle]
pub unsafe extern "C" fn art_task_create(
    handle: *mut u32,
    entry: Option<Entry>,
    arg: *mut c_void,
    stack: *mut u8,
    stack_size: usize,
    priority: u8,
) -> i32 {
    let entry = match entry {
        Some(entry) if !handle.is_null() => entry,
        _ => return ERROR,
    };
    code(task::spawn(entry, arg, stack, stack_size, priority, handle))
}

#[no_mangle]
pub extern "C" fn art_task_exit() -> ! {
    task::exit_current()
}

#[no_mangle]
pub extern "C" fn art_task_delete(handle: u32) -> i32 {
    code(task::delete(handle))
}

#[no_mangle]
pub extern "C" fn art_task_self() -> u32 {
    task::current_handle()
}

#[no_mangle]
pub extern "C" fn art_task_yield() {
    task::yield_now()
}

/// Blocks the running task until the clock reaches `deadline`.
#[no_mangle]
pub extern "C" fn art_task_sleep_until(deadline: u64) {
    task::sleep_until(deadline)
}

/// Blocks the running task for `ns` nanoseconds.
#[no_mangle]
pub extern "C" fn art_task_sleep(ns: u64) {
    task::sleep_until(time::now().saturating_add(ns))
}

#[no_mangle]
pub extern "C" fn art_task_set_priority(handle: u32, priority: u8) -> i32 {
    code(task::set_priority(handle, priority))
}

#[no_mangle]
pub unsafe extern "C" fn art_task_get_priority(handle: u32, priority: *mut u8) -> i32 {
    match task::get_priority(handle) {
        Ok(p) => {
            priority.write(p);
            OK
        }
        Err(e) => code(Err(e)),
    }
}

#[no_mangle]
pub extern "C" fn art_task_set_tag(handle: u32, tag: usize) -> i32 {
    code(task::set_tag(handle, tag))
}

/// The tag of a task, or zero if the handle is not valid.
#[no_mangle]
pub extern "C" fn art_task_get_tag(handle: u32) -> usize {
    task::get_tag(handle).unwrap_or(0)
}

/// Whether a handle names a live task.
#[no_mangle]
pub extern "C" fn art_task_valid(handle: u32) -> bool {
    task::get_priority(handle).is_ok()
}

/// Lets the runtime notice deadlines that have passed. A task that runs for a
/// long time without another runtime call should call this.
#[no_mangle]
pub extern "C" fn art_poll() {
    task::poll()
}

// ---- Kernel calls for a program that manages containers ----

use crate::sys;

/// Maps `pages` zeroed pages at the address `va`. Returns 0, or -1 if the
/// kernel refuses.
#[no_mangle]
pub extern "C" fn art_sys_map(va: usize, pages: usize) -> i32 {
    if sys::map_pages(va, pages) {
        0
    } else {
        -1
    }
}

#[no_mangle]
pub extern "C" fn art_sys_unmap(va: usize, pages: usize) -> i64 {
    sys::unmap_pages(va, pages) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_new_container(
    va: usize,
    pages: usize,
    funding: usize,
    process_quota: usize,
    cpu: usize,
    endpoint: i64,
    entry: usize,
    sp: usize,
) -> i64 {
    let endpoint = if endpoint < 0 { None } else { Some(endpoint as usize) };
    sys::new_container(va, pages, funding, process_quota, cpu, endpoint, entry, sp) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_send_pages(endpoint: usize, va: usize, pages: usize) -> i64 {
    sys::send_pages(endpoint, va, pages) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_new_thread(entry: usize, sp: usize) -> i64 {
    sys::new_thread(entry, sp) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_thread_exit() -> ! {
    sys::thread_exit()
}

#[no_mangle]
pub extern "C" fn art_sys_cpu_online_child(child: usize, cpu: usize) -> i64 {
    sys::cpu_online_child(child, cpu) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_cpu_offline_request(cpu: usize) -> i64 {
    sys::cpu_offline_request(cpu) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_cpu_offline_request_child(child: usize, cpu: usize) -> i64 {
    sys::cpu_offline_request_child(child, cpu) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_kill_container(child: usize) -> i64 {
    sys::kill_container(child) as i64
}

#[no_mangle]
pub extern "C" fn art_sys_yield() {
    sys::yield_cpu()
}

/// Copies the name of a kernel result into `buf` as a C string.
#[no_mangle]
pub unsafe extern "C" fn art_sys_error_name(code: i64, buf: *mut u8, size: usize) {
    if buf.is_null() || size == 0 {
        return;
    }
    let name = sys::error_name(code as isize).as_bytes();
    let n = name.len().min(size - 1);
    core::ptr::copy_nonoverlapping(name.as_ptr(), buf, n);
    buf.add(n).write(0);
}

/// Reads the timestamp counter.
#[no_mangle]
pub extern "C" fn art_sys_rdtsc() -> u64 {
    sys::rdtsc()
}
