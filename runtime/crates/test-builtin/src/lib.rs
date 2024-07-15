use spidermonkey_rs::conversions::ToJSValConvertible;
use spidermonkey_rs::raw::{JS_DefineFunction, JSContext};
use spidermonkey_rs::raw::JS::{CallArgs, Value};
use spidermonkey_rs::rust::{describe_scripted_caller, MutableHandle};
use starlingmonkey_rs::Engine;

#[no_mangle]
pub unsafe extern "C" fn builtin_test_builtin_install(engine: &mut Engine) -> bool {
    JS_DefineFunction(engine.cx(), engine.global(), c"describe_caller".as_ptr(),
                      Some(describe_caller), 0, 0);
    println!("test_builtin_install called with engine: {:?}", engine);
    true
}

unsafe extern "C" fn describe_caller(cx: *mut JSContext, argc: u32, vp: *mut Value) -> bool {
    let args = CallArgs::from_vp(vp, argc);
    let caller = describe_scripted_caller(cx).unwrap();
    let out = format!("describe_caller called by: {}:{}", caller.filename, caller.line);
    out.to_jsval(cx, MutableHandle::from_raw(args.rval()));
    true
}
