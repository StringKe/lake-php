//! Boundary types. Handles and out-parameters cross the ABI as `Option<&T>` / `Option<&mut T>` / `Option<Box<T>>`,
//! which Rust guarantees to be laid out as nullable C pointers, so they need no `unsafe` to use.
//! Only borrowed byte ranges (`LakeStr`, parameter and cell arrays) and the release of `LakeOwned` remain `unsafe`.

use std::any::Any;
use std::ffi::c_char;
use std::io::Write;
use std::panic::{self, AssertUnwindSafe};
use std::sync::{Mutex, Once};

use tokio::runtime::Runtime;

use crate::error::{Failure, GENERAL, INVALID_TEXT};

#[repr(C)]
pub struct LakeStr {
    pub ptr: *const u8,
    pub len: usize,
}

impl LakeStr {
    /// # Safety
    /// `ptr` must be null or point to `len` bytes that stay readable for `'a`.
    pub unsafe fn bytes<'a>(&self) -> Option<&'a [u8]> {
        if self.ptr.is_null() { None } else { Some(unsafe { std::slice::from_raw_parts(self.ptr, self.len) }) }
    }

    /// # Safety
    /// Same as [`LakeStr::bytes`].
    pub unsafe fn text<'a>(&self, what: &str) -> Result<Option<&'a str>, Failure> {
        match unsafe { self.bytes() } {
            None => Ok(None),
            Some(bytes) => std::str::from_utf8(bytes).map(Some).map_err(|_| Failure::new(INVALID_TEXT, format!("{what} is not valid UTF-8"))),
        }
    }

    /// # Safety
    /// Same as [`LakeStr::bytes`].
    pub unsafe fn required<'a>(&self, what: &str) -> Result<&'a str, Failure> {
        unsafe { self.text(what) }?.ok_or_else(|| Failure::new(GENERAL, format!("{what} is required")))
    }
}

/// # Safety
/// `ptr` must be null or point to `len` readable items that stay valid for `'a`.
pub unsafe fn slice<'a, T>(ptr: *const T, len: usize) -> &'a [T] {
    if ptr.is_null() || len == 0 { &[] } else { unsafe { std::slice::from_raw_parts(ptr, len) } }
}

/// A NUL-terminated byte string allocated here; the C side releases it with `lake_owned_free`.
#[repr(C)]
pub struct LakeOwned {
    pub ptr: *mut u8,
    pub len: usize,
}

impl LakeOwned {
    pub fn from_string(value: String) -> Self {
        let mut bytes = value.into_bytes();
        bytes.push(0);
        let len = bytes.len() - 1;
        let ptr = Box::into_raw(bytes.into_boxed_slice()).cast::<u8>();
        Self { ptr, len }
    }

    pub fn empty() -> Self {
        Self { ptr: std::ptr::null_mut(), len: 0 }
    }

    fn release(&mut self) {
        if !self.ptr.is_null() {
            // SAFETY: a non-null `ptr` only ever comes from `from_string`, which leaked a boxed slice of `len + 1` bytes.
            drop(unsafe { Box::from_raw(std::ptr::slice_from_raw_parts_mut(self.ptr, self.len + 1)) });
        }
        *self = Self::empty();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_owned_free(owned: Option<&mut LakeOwned>) {
    if let Some(owned) = owned {
        owned.release();
    }
}

pub fn write_owned(out: Option<&mut LakeOwned>, value: String) {
    if let Some(out) = out {
        out.release();
        *out = LakeOwned::from_string(value);
    }
}

#[repr(C)]
pub struct LakeError {
    pub sqlstate: [c_char; 6],
    pub code: i64,
    pub message: LakeOwned,
}

#[unsafe(no_mangle)]
pub extern "C" fn lake_error_clear(error: Option<&mut LakeError>) {
    if let Some(error) = error {
        error.message.release();
        error.sqlstate = [0; 6];
        error.code = 0;
    }
}

fn set_error(error: Option<&mut LakeError>, failure: Failure) {
    let Some(error) = error else { return };
    error.message.release();
    let mut sqlstate = [0 as c_char; 6];
    for (slot, byte) in sqlstate.iter_mut().zip(failure.sqlstate.bytes().take(5)) {
        *slot = byte as c_char;
    }
    error.sqlstate = sqlstate;
    error.code = failure.code;
    error.message = LakeOwned::from_string(failure.message);
}

/// Replaces the default hook, whose multi-line output and backtrace hint do not fit a php-fpm log, with one line on
/// stderr. It also covers panics on runtime threads, which never pass through `guard`.
fn install_panic_hook() {
    static HOOK: Once = Once::new();
    HOOK.call_once(|| panic::set_hook(Box::new(log_panic)));
}

fn log_panic(info: &panic::PanicHookInfo<'_>) {
    let thread = std::thread::current();
    let location = info.location().map(|l| format!("{}:{}:{}", l.file(), l.line(), l.column())).unwrap_or_else(|| "unknown location".to_string());
    let line = format!("pdo_lake panic: thread '{}' at {location}: {}\n", thread.name().unwrap_or("<unnamed>"), panic_detail(info.payload()).replace('\n', "\\n"));
    // A failing write must not panic again inside the hook, which would abort the process.
    let _ = std::io::stderr().lock().write_all(line.as_bytes());
}

fn panic_detail(payload: &(dyn Any + Send)) -> String {
    payload.downcast_ref::<&str>().map(|s| s.to_string()).or_else(|| payload.downcast_ref::<String>().cloned()).unwrap_or_default()
}

/// Runs `body`, turning both errors and panics into a filled `LakeError` and the `fallback` value.
pub fn guard<T>(error: Option<&mut LakeError>, fallback: T, body: impl FnOnce() -> Result<T, Failure>) -> T {
    install_panic_hook();
    match panic::catch_unwind(AssertUnwindSafe(body)) {
        Ok(Ok(value)) => value,
        Ok(Err(failure)) => {
            set_error(error, failure);
            fallback
        }
        Err(payload) => {
            set_error(error, Failure::new(GENERAL, format!("pdo_lake internal error: {}", panic_detail(payload.as_ref()))));
            fallback
        }
    }
}

/// Runs a release path, catching panics so they never unwind into C; the hook still logs them.
pub fn quietly(body: impl FnOnce()) {
    install_panic_hook();
    let _ = panic::catch_unwind(AssertUnwindSafe(body));
}

/// The driver is async only; every call blocks the calling PHP thread on this runtime.
///
/// A forked child cannot use the parent's runtime threads, so the runtime is rebuilt per process id and the
/// inherited one is leaked instead of dropped (dropping would wait for threads that do not exist in the child).
pub fn runtime() -> &'static Runtime {
    static RUNTIME: Mutex<Option<(u32, &'static Runtime)>> = Mutex::new(None);
    let pid = std::process::id();
    let mut slot = RUNTIME.lock().unwrap_or_else(|e| e.into_inner());
    if let Some((owner, runtime)) = *slot
        && owner == pid
    {
        return runtime;
    }
    let runtime = tokio::runtime::Builder::new_multi_thread().worker_threads(1).thread_name("pdo-lake").enable_all().build().expect("failed to start the pdo_lake runtime");
    let runtime: &'static Runtime = Box::leak(Box::new(runtime));
    *slot = Some((pid, runtime));
    runtime
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn guard_turns_a_formatted_panic_into_an_error() {
        let mut error = LakeError { sqlstate: [0; 6], code: 0, message: LakeOwned::empty() };
        let value = guard(Some(&mut error), -1, || -> Result<i32, Failure> { panic!("broken row {}", 7) });
        assert_eq!(value, -1);
        // SAFETY: `guard` filled `message` through `LakeOwned::from_string`.
        let message = unsafe { std::str::from_utf8(std::slice::from_raw_parts(error.message.ptr, error.message.len)) }.unwrap();
        assert_eq!(message, "pdo_lake internal error: broken row 7");
        lake_error_clear(Some(&mut error));
    }
}
