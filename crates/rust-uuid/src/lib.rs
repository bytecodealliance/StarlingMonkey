use std::ffi::CString;
use std::os::raw::c_char;

use uuid::Uuid;

/// Generate a new UUID (version 4) and return it as a C-compatible string.
/// Caller is responsible for freeing the returned string using `free_uuid`
/// function.
#[no_mangle]
pub extern "C" fn new_uuid_v4() -> *mut c_char {
    let uuid = Uuid::new_v4().to_string();
    CString::new(uuid).unwrap().into_raw()
}

/// Free a C string allocated by Rust.
#[no_mangle]
pub extern "C" fn free_uuid(s: *mut c_char) {
    if !s.is_null() {
        let _ = unsafe { CString::from_raw(s) };
    }
}
