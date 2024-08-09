/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO: investigate copying more of SpiderMonkey's `mozglue/static/rust/lib.rs`

// Ensure that the encoding modules are built, as SpiderMonkey relies on them.
pub use encoding_c;
pub use encoding_c_mem;

// The jsimpls module just implements traits so can be private
mod jsimpls;

// Modules with public definitions
pub mod jsgc;
pub mod jsid;
pub mod jsval;
pub mod jsapi;

/// Configure a panic hook to redirect rust panics to MFBT's MOZ_Crash.
/// See <https://searchfox.org/mozilla-esr115/source/mozglue/static/rust/lib.rs#106>
#[no_mangle]
pub extern "C" fn install_rust_hooks() {
    //std::panic::set_hook(Box::new(panic_hook));
    #[cfg(feature = "oom_with_hook")]
    oom_hook::install();
}

#[cfg(feature = "oom_with_hook")]
mod oom_hook {
    use std::alloc::{set_alloc_error_hook, Layout};

    extern "C" {
        pub fn RustHandleOOM(size: usize) -> !;
    }

    pub fn hook(layout: Layout) {
        unsafe {
            RustHandleOOM(layout.size());
        }
    }

    pub fn install() {
        set_alloc_error_hook(hook);
    }
}
