/// Wrapper for the Url crate and a URLSearchParams implementation that enables use from C++.
use std::marker::PhantomData;
use std::slice;
use url::{form_urlencoded, quirks, Url};

pub struct JSUrl {
    url: Url,
    params: *mut JSUrlSearchParams,
}

impl JSUrl {
    fn update_params(&self) {
        if let Some(params) = unsafe { self.params.as_mut() } {
            params.list = self.url.query_pairs().into_owned().collect();
        }
    }
}

enum UrlOrString {
    Url(*mut JSUrl),
    Str(String),
}

pub struct JSUrlSearchParams {
    list: Vec<(String, String)>,
    url_or_str: UrlOrString,
}

impl JSUrlSearchParams {
    /// Update the associated Url object or string representation after params change.
    /// Per WHATWG URL spec, the URL underlying a URLSearchParams object needs to be
    /// updated as part of any mutations of the URLSearchParams.
    /// Additionally, we keep around an updated string representation of the parameters.
    /// This is used in `params_to_string` to hand out a stable reference.
    fn update_url_or_str(&mut self) {
        match self.url_or_str {
            UrlOrString::Url(url) => {
                let url = unsafe { url.as_mut().unwrap() };
                if self.list.is_empty() {
                    url.url.set_query(None);
                } else {
                    let mut pairs = url.url.query_pairs_mut();
                    pairs.clear();
                    pairs.extend_pairs(self.list.iter());
                }
            }
            UrlOrString::Str(_) => {
                let str;
                if self.list.is_empty() {
                    str = form_urlencoded::Serializer::new(String::new()).finish();
                } else {
                    str = form_urlencoded::Serializer::new(String::new())
                        .extend_pairs(&*self.list)
                        .finish();
                }
                self.url_or_str = UrlOrString::Str(str);
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn new_jsurl(spec: &SpecString) -> *mut JSUrl {
    match Url::parse(spec.into()) {
        Ok(url) => Box::into_raw(Box::new(JSUrl {
            url,
            params: std::ptr::null_mut(),
        })),
        _ => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn new_jsurl_with_base(spec: &SpecString, base: &JSUrl) -> *mut JSUrl {
    match base.url.join(spec.into()) {
        Ok(url) => Box::into_raw(Box::new(JSUrl {
            url,
            params: std::ptr::null_mut(),
        })),
        _ => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn authority(url: &JSUrl) -> SpecSlice {
    url.url.authority().into()
}

#[no_mangle]
pub extern "C" fn path_with_query(url: &JSUrl) -> SpecSlice {
    let mut slice: SpecSlice = url.url.path().into();
    if let Some(query) = url.url.query() {
        slice.len += 1 + query.len();
    }
    slice
}

#[no_mangle]
pub extern "C" fn hash(url: &JSUrl) -> SpecSlice {
    quirks::hash(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_hash(url: &mut JSUrl, hash: &SpecString) {
    quirks::set_hash(&mut url.url, hash.into());
}

#[no_mangle]
pub extern "C" fn host(url: &JSUrl) -> SpecSlice {
    quirks::host(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_host(url: &mut JSUrl, host: &SpecString) {
    let _ = quirks::set_host(&mut url.url, host.into());
}

#[no_mangle]
pub extern "C" fn hostname(url: &JSUrl) -> SpecSlice {
    quirks::hostname(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_hostname(url: &mut JSUrl, hostname: &SpecString) {
    let _ = quirks::set_hostname(&mut url.url, hostname.into());
}

#[no_mangle]
pub extern "C" fn href(url: &JSUrl) -> SpecSlice {
    quirks::href(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_href(url: &mut JSUrl, href: &SpecString) {
    let _ = quirks::set_href(&mut url.url, href.into());
    url.update_params();
}

#[no_mangle]
pub extern "C" fn origin(url: &JSUrl) -> SpecString {
    quirks::origin(&url.url).into()
}

#[no_mangle]
pub extern "C" fn password(url: &JSUrl) -> SpecSlice {
    quirks::password(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_password(url: &mut JSUrl, password: &SpecString) {
    let _ = quirks::set_password(&mut url.url, password.into());
}

#[no_mangle]
pub extern "C" fn pathname(url: &JSUrl) -> SpecSlice {
    quirks::pathname(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_pathname(url: &mut JSUrl, pathname: &SpecString) {
    quirks::set_pathname(&mut url.url, pathname.into());
}

#[no_mangle]
pub extern "C" fn port(url: &JSUrl) -> SpecSlice {
    quirks::port(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_port(url: &mut JSUrl, port: &SpecString) {
    let _ = quirks::set_port(&mut url.url, port.into());
}

#[no_mangle]
pub extern "C" fn protocol(url: &JSUrl) -> SpecSlice {
    quirks::protocol(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_protocol(url: &mut JSUrl, protocol: &SpecString) {
    let _ = quirks::set_protocol(&mut url.url, protocol.into());
}

#[no_mangle]
pub extern "C" fn search(url: &JSUrl) -> SpecSlice {
    quirks::search(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_search(url: &mut JSUrl, search: &SpecString) {
    let _ = quirks::set_search(&mut url.url, search.into());
    url.update_params();
}

#[no_mangle]
pub extern "C" fn username(url: &JSUrl) -> SpecSlice {
    quirks::username(&url.url).into()
}

#[no_mangle]
pub extern "C" fn set_username(url: &mut JSUrl, username: &SpecString) {
    let _ = quirks::set_username(&mut url.url, username.into());
}

#[no_mangle]
pub unsafe extern "C" fn url_search_params(url: *mut JSUrl) -> *mut JSUrlSearchParams {
    let url = url.as_mut().unwrap();
    if url.params.is_null() {
        url.params = Box::into_raw(Box::new(JSUrlSearchParams {
            list: url.url.query_pairs().into_owned().collect(),
            url_or_str: UrlOrString::Url(url),
        }));
    }
    url.params
}

#[no_mangle]
pub unsafe extern "C" fn new_params() -> *mut JSUrlSearchParams {
    Box::into_raw(Box::new(JSUrlSearchParams {
        list: Vec::new(),
        url_or_str: UrlOrString::Str("".to_owned()),
    }))
}

#[no_mangle]
pub extern "C" fn params_init(params: &mut JSUrlSearchParams, init: &SpecString) {
    let init = unsafe { slice::from_raw_parts(init.data, init.len) };

    // https://url.spec.whatwg.org/#dom-urlsearchparams-urlsearchparams
    // Step 1
    let init = if init.len() > 0 && init[0] == '?' as u8 {
        &init[1..]
    } else {
        init
    };

    params.list = form_urlencoded::parse(init).into_owned().collect();
    params.update_url_or_str();
}

#[no_mangle]
pub extern "C" fn params_append(
    params: &mut JSUrlSearchParams,
    name: SpecString,
    value: SpecString,
) {
    params.list.push((name.into(), value.into()));
    params.update_url_or_str();
}

#[no_mangle]
pub extern "C" fn params_delete(params: &mut JSUrlSearchParams, name: &SpecString) {
    let name: &str = name.into();
    params.list.retain(|&(ref k, _)| k != &name);
    params.update_url_or_str();
}

#[no_mangle]
pub extern "C" fn params_has(params: &JSUrlSearchParams, name: &SpecString) -> bool {
    let name: &str = name.into();
    params.list.iter().find(|&kv| kv.0 == name).is_some()
}

#[no_mangle]
pub extern "C" fn params_get<'a>(
    params: &'a JSUrlSearchParams,
    name: &SpecString,
) -> SpecSlice<'a> {
    let name: &str = name.into();
    params
        .list
        .iter()
        .find(|&kv| kv.0 == name)
        .map(|ref kv| SpecSlice::from(kv.1.as_str()))
        .unwrap_or_else(|| SpecSlice::new(std::ptr::null(), 0))
}

#[no_mangle]
pub extern "C" fn params_at<'a>(
    params: &'a JSUrlSearchParams,
    index: usize,
    param_out: &mut JSSearchParam<'a>,
) {
    if let Some((name, value)) = params.list.get(index) {
        param_out.done = false;
        param_out.name = SpecSlice::from(name.as_str());
        param_out.value = SpecSlice::from(value.as_str());
    } else {
        param_out.done = true;
    }
}

#[no_mangle]
pub extern "C" fn params_get_all<'a>(
    params: &'a JSUrlSearchParams,
    name: &SpecString,
) -> CVec<SpecSlice<'a>> {
    let name: &str = name.into();
    let mut values: Vec<SpecSlice> = params
        .list
        .iter()
        .filter_map(|&(ref k, ref v)| {
            if k == &name {
                Some(SpecSlice::from(v.as_str()))
            } else {
                None
            }
        })
        .collect();

    let output = CVec {
        ptr: values.as_mut_ptr(),
        len: values.len(),
        cap: values.capacity(),
    };
    std::mem::forget(values);
    output
}

#[no_mangle]
pub extern "C" fn params_set(params: &mut JSUrlSearchParams, name: SpecString, value: SpecString) {
    let name: String = name.into();
    let value: String = value.into();

    let mut index = None;
    let mut i = 0;
    params.list.retain(|&(ref k, _)| {
        if index.is_none() {
            if k == &name {
                index = Some(i);
            } else {
                i += 1;
            }
            true
        } else {
            k != &name
        }
    });
    match index {
        Some(index) => params.list[index].1 = value,
        None => params.list.push((name, value)),
    };

    params.update_url_or_str();
}

#[no_mangle]
pub extern "C" fn params_sort(params: &mut JSUrlSearchParams) {
    params
        .list
        .sort_by(|(a, _), (b, _)| a.encode_utf16().cmp(b.encode_utf16()));
    params.update_url_or_str();
}

#[no_mangle]
pub extern "C" fn params_to_string<'a>(params: &'a JSUrlSearchParams) -> SpecSlice<'a> {
    match &params.url_or_str {
        UrlOrString::Url(url) => {
            let url = unsafe { url.as_mut().unwrap() };
            match url.url.query() {
                Some(query) => query.into(),
                None => "".into(),
            }
        }
        UrlOrString::Str(str) => str.as_str().into(),
    }
}

#[repr(C)]
pub struct CVec<T> {
    ptr: *mut T,
    len: usize,
    cap: usize,
}

impl<T> Drop for CVec<T> {
    fn drop(&mut self) {
        unsafe {
            self.ptr.drop_in_place();
        }
    }
}

#[repr(C)]
pub struct JSSearchParam<'a> {
    name: SpecSlice<'a>,
    value: SpecSlice<'a>,
    done: bool,
}
/// This type exists to transfer String-likes over FFI.
#[repr(C)]
pub struct SpecString {
    data: *mut u8,
    len: usize,
    cap: usize,
}

impl From<String> for SpecString {
    fn from(mut s: String) -> SpecString {
        let data = s.as_mut_ptr();
        let len = s.len();
        let cap = s.capacity();
        std::mem::forget(s);
        Self { data, len, cap }
    }
}

impl From<SpecString> for String {
    fn from(spec: SpecString) -> String {
        let spec = unsafe { slice::from_raw_parts(spec.data, spec.len) };
        String::from_utf8(spec.to_owned()).unwrap()
    }
}

impl<'a> From<&'a SpecString> for &'a str {
    fn from(spec: &SpecString) -> &str {
        let spec = unsafe { slice::from_raw_parts(spec.data, spec.len) };
        std::str::from_utf8(spec).unwrap()
    }
}

impl Drop for SpecString {
    fn drop(&mut self) {
        unsafe {
            self.data.drop_in_place();
        }
    }
}

/// This type exists to transfer &str-likes over FFI.
#[repr(C)]
pub struct SpecSlice<'a> {
    data: *const u8,
    len: usize,
    _marker: PhantomData<&'a [u8]>,
}

impl SpecSlice<'_> {
    fn new(data: *const u8, len: usize) -> Self {
        SpecSlice {
            data,
            len: len,
            _marker: PhantomData,
        }
    }
}

impl<'a> From<&'a str> for SpecSlice<'a> {
    fn from(s: &'a str) -> SpecSlice<'a> {
        let ptr = if s.len() > 0 {
            s.as_ptr()
        } else {
            std::ptr::null()
        };
        SpecSlice::new(ptr, s.len())
    }
}

impl<'a> From<&SpecSlice<'a>> for &'a str {
    fn from(spec: &SpecSlice<'a>) -> &'a str {
        let spec = unsafe { slice::from_raw_parts(spec.data, spec.len) };
        std::str::from_utf8(spec).unwrap()
    }
}
