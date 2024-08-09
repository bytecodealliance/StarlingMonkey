/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is copied with slight changes from Gecko:
// https://searchfox.org/mozilla-central/rev/91030e74f3e9bb2aac9fe2cbff80734c6fd610b9/mozglue/static/rust/lib.rs

use arrayvec::ArrayString;
use std::cmp;
use std::ops::Deref;
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::panic;
#[cfg(feature = "panic_hook")]
use std::panic::PanicHookInfo;
#[cfg(not(feature = "panic_hook"))]
use std::panic::PanicInfo as PanicHookInfo;

#[link(name = "wrappers")]
extern "C" {
    // We can't use MOZ_Crash directly because it may be weakly linked
    // and rust can't handle that.
    fn RustMozCrash(filename: *const c_char, line: c_int, reason: *const c_char) -> !;
}

/// Truncate a string at the closest unicode character boundary
/// ```
/// assert_eq!(str_truncate_valid("éà", 3), "é");
/// assert_eq!(str_truncate_valid("éà", 4), "éè");
/// ```
fn str_truncate_valid(s: &str, mut mid: usize) -> &str {
    loop {
        if let Some(res) = s.get(..mid) {
            return res;
        }
        mid -= 1;
    }
}

/// Similar to ArrayString, but with terminating nul character.
#[derive(Debug, PartialEq)]
struct ArrayCString<const CAP: usize> {
    inner: ArrayString<CAP>,
}

impl<S: AsRef<str>, const CAP: usize> From<S> for ArrayCString<CAP> {
    /// Contrary to ArrayString::from, truncates at the closest unicode
    /// character boundary.
    /// ```
    /// assert_eq!(ArrayCString::<4>::from("éà"),
    ///            ArrayCString::<4>::from("é"));
    /// assert_eq!(&*ArrayCString::<4>::from("éà"), "é\0");
    /// ```
    fn from(s: S) -> Self {
        let s = s.as_ref();
        let len = cmp::min(s.len(), CAP - 1);
        let mut result = Self {
            inner: ArrayString::from(str_truncate_valid(s, len)).unwrap(),
        };
        result.inner.push('\0');
        result
    }
}

impl<const CAP: usize> Deref for ArrayCString<CAP> {
    type Target = str;

    fn deref(&self) -> &str {
        self.inner.as_str()
    }
}

fn panic_hook(info: &PanicHookInfo) {
    // Try to handle &str/String payloads, which should handle 99% of cases.
    let payload = info.payload();
    let message = if let Some(s) = payload.downcast_ref::<&str>() {
        s
    } else if let Some(s) = payload.downcast_ref::<String>() {
        s.as_str()
    } else {
        // Not the most helpful thing, but seems unlikely to happen
        // in practice.
        "Unhandled rust panic payload!"
    };
    let (filename, line) = if let Some(loc) = info.location() {
        (loc.file(), loc.line())
    } else {
        ("unknown.rs", 0)
    };
    // Copy the message and filename to the stack in order to safely add
    // a terminating nul character (since rust strings don't come with one
    // and RustMozCrash wants one).
    let message = ArrayCString::<512>::from(message);
    let filename = ArrayCString::<512>::from(filename);
    unsafe {
        RustMozCrash(
            filename.as_ptr() as *const c_char,
            line as c_int,
            message.as_ptr() as *const c_char,
        );
    }
}

/// Configure a panic hook to redirect rust panics to MFBT's MOZ_Crash.
#[no_mangle]
pub extern "C" fn install_rust_hooks() {
    panic::set_hook(Box::new(panic_hook));
    #[cfg(feature = "panic_hook")]
    use std::alloc::set_alloc_error_hook;
    #[cfg(feature = "panic_hook")]
    set_alloc_error_hook(oom_hook::hook);
}

#[cfg(feature = "panic_hook")]
mod oom_hook {

    extern "C" {
        pub fn RustHandleOOM(size: usize) -> !;
    }

    pub fn hook(layout: Layout) {
        unsafe {
            RustHandleOOM(layout.size());
        }
    }
}
