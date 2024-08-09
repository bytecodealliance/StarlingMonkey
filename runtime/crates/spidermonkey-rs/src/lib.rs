/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#![crate_name = "spidermonkey_rs"]
#![crate_type = "rlib"]
#![allow(
    non_upper_case_globals,
    non_camel_case_types,
    non_snake_case,
    improper_ctypes
)]

//!
//! This crate contains Rust bindings to the [SpiderMonkey Javascript engine][1]
//! developed by Mozilla.
//!
//! These bindings are designed to be a fairly straightforward translation to the C++ API, while
//! taking advantage of Rust's memory safety. For more about the Spidermonkey API, see the
//! [API Reference][2] and the [User Guide][3] on MDN, and the [embedding examples][4] on GitHub.
//!
//! The code from User Guide sections [A minimal example](https://github.com/servo/mozjs/blob/main/mozjs/examples/minimal.rs) and
//! [Running scripts](https://github.com/servo/mozjs/blob/main/mozjs/examples/eval.rs) are also included.
//!
//! [1]: https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey
//! [2]: https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference
//! [3]: https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/JSAPI_User_Guide
//! [4]: https://github.com/mozilla-spidermonkey/spidermonkey-embedding-examples/
//!

pub mod raw {
    pub use jsapi_rs::jsapi::*;
}

#[macro_use]
pub mod rust;

mod consts;
pub mod conversions;
pub mod error;
pub mod gc;
pub mod panic;
pub mod typedarray;

pub use crate::consts::*;
pub use jsapi_rs::jsid;
pub use jsapi_rs::jsval;
