use std::ffi::CStr;
use std::os::raw::c_char;

use crate::MultipartParser;

/// A slice of bytes as seen from C.
#[repr(C)]
pub struct Slice {
    pub data: *const u8,
    pub len: usize,
}

/// A C view of a parsed entry. For optional fields, a NULL data pointer means not present.
#[repr(C)]
pub struct Entry {
    pub name: Slice,
    pub value: Slice,
    pub filename: Slice,
    pub content_type: Slice,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum RetCode {
    Ok = 0,
    Eos = 1,
    Error = 2,
}

pub struct State {
    inner: MultipartParser<'static>,
}

/// Crates a new parser with data provided.
///
/// # Safety
///
/// The caller must ensure that the input data remains valid for the entire
/// lifetime of the parser's state.
#[no_mangle]
pub unsafe extern "C" fn multipart_parser_new(
    data: *mut Slice,
    boundary: *const c_char,
) -> *mut State {
    if data.is_null() || boundary.is_null() {
        return std::ptr::null_mut();
    }

    let data_slice = std::slice::from_raw_parts((*data).data, (*data).len);
    let data_static = std::mem::transmute::<&[u8], &'static [u8]>(data_slice);

    let boundary = match CStr::from_ptr(boundary).to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };

    let inner = MultipartParser::new(data_static, boundary);
    let state = State { inner };

    Box::into_raw(Box::new(state))
}

/// Free parser created by `multipart_parser_new`.
///
/// # Safety
///
/// The caller must ensure that the state is a valid parser pointer.
#[no_mangle]
pub unsafe extern "C" fn multipart_parser_free(state: *mut State) {
    if state.is_null() {
        return;
    }

    let _ = Box::from_raw(state);
}

/// Retrieve the next entry from the parser into provided entry.
///
/// # Safety
///
/// The caller must ensure that the state and entry are valid pointers.
#[no_mangle]
pub unsafe extern "C" fn multipart_parser_next(
    state: *mut State,
    entry: *mut Entry,
) -> RetCode {
    if state.is_null() || entry.is_null() {
        return RetCode::Error;
    }

    let state = &mut *state;

    match state.inner.parse_next() {
        Some(Ok(e)) => {
            (*entry).name = Slice {
                data: e.name().as_ptr(),
                len: e.name().len(),
            };
            (*entry).value = Slice {
                data: e.value().as_ptr(),
                len: e.value().len(),
            };
            (*entry).filename = match e.filename() {
                Some(f) => Slice {
                    data: f.as_ptr(),
                    len: f.len(),
                },
                None => Slice {
                    data: std::ptr::null(),
                    len: 0,
                },
            };
            (*entry).content_type = match e.content_type() {
                Some(ct) => Slice {
                    data: ct.as_ptr(),
                    len: ct.len(),
                },
                None => Slice {
                    data: std::ptr::null(),
                    len: 0,
                },
            };
            RetCode::Ok
        }
        Some(Err(_)) => RetCode::Error,
        None => RetCode::Eos,
    }
}

#[no_mangle]
/// Retrieve the boundary from content-type header
///
/// # Safety
///
/// The caller must ensure that the content_type and boundary are valid pointers.
pub unsafe extern "C" fn boundary_from_content_type(
    content_type: *mut Slice,
    boundary: *mut Slice,
) {
    if content_type.is_null() || boundary.is_null() {
        return;
    }

    let data_slice = std::slice::from_raw_parts((*content_type).data, (*content_type).len);

    match crate::boundary_from_content_type(data_slice) {
        Some(b) => {
            (*boundary).data = b.as_ptr();
            (*boundary).len = b.len();
        }
        None => {
            (*boundary).data = std::ptr::null();
            (*boundary).len = 0;
        }
    }
}
