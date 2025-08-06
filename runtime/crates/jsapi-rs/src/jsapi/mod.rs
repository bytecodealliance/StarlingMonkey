// SpiderMonkey's headers are different between release and debug builds,
// so jsapi-rs has two sets of bindings.
#[cfg_attr(feature = "debugmozjs", path = "bindings_debug.rs")]
#[cfg_attr(not(feature = "debugmozjs"), path = "bindings_release.rs")]
mod bindings;
pub use bindings::root::*;
