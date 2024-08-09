/* automatically generated by rust-bindgen 0.69.4 */

#[allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]
pub mod root {
    #[allow(unused_imports)]
    use self::super::root;
    pub use spidermonkey_rs::raw::*;
    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub struct _IO_FILE {
        _unused: [u8; 0],
    }
    pub type FILE = root::_IO_FILE;
    #[repr(C)]
    #[derive(Debug, Copy, Clone, PartialEq)]
    pub struct __mbstate_t {
        pub __opaque1: ::std::os::raw::c_uint,
        pub __opaque2: ::std::os::raw::c_uint,
    }
    pub mod api {
        #[allow(unused_imports)]
        use self::super::super::root;
        pub mod Errors {
            #[allow(unused_imports)]
            use self::super::super::super::root;
            extern "C" {
                #[link_name = "\u{1}_ZN3api6ErrorsL13WrongReceiverE"]
                pub static WrongReceiver: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL13NoCtorBuiltinE"]
                pub static NoCtorBuiltin: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL9TypeErrorE"]
                pub static TypeError: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL20CtorCalledWithoutNewE"]
                pub static CtorCalledWithoutNew: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL15InvalidSequenceE"]
                pub static InvalidSequence: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL13InvalidBufferE"]
                pub static InvalidBuffer: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL15ForEachCallbackE"]
                pub static ForEachCallback: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL18RequestHandlerOnlyE"]
                pub static RequestHandlerOnly: root::JSErrorFormatString;
                #[link_name = "\u{1}_ZN3api6ErrorsL18InitializationOnlyE"]
                pub static InitializationOnly: root::JSErrorFormatString;
            }
        }
        pub type PollableHandle = i32;
        pub const INVALID_POLLABLE_HANDLE: root::api::PollableHandle = -1;
        #[repr(C)]
        #[derive(Debug, Copy, Clone, PartialEq)]
        pub struct Engine {
            pub _address: u8,
        }
        impl Engine {
            #[inline]
            pub unsafe fn cx(&mut self) -> *mut root::JSContext {
                Engine_cx(self)
            }
            #[inline]
            pub unsafe fn global(&mut self) -> root::JS::HandleObject {
                Engine_global(self)
            }
            #[inline]
            pub unsafe fn initialize(
                &mut self,
                filename: *const ::std::os::raw::c_char,
            ) -> bool {
                Engine_initialize(self, filename)
            }
            #[inline]
            pub unsafe fn define_builtin_module(
                &mut self,
                id: *const ::std::os::raw::c_char,
                builtin: root::JS::HandleValue,
            ) -> bool {
                Engine_define_builtin_module(self, id, builtin)
            }
            #[inline]
            pub unsafe fn enable_module_mode(&mut self, enable: bool) {
                Engine_enable_module_mode(self, enable)
            }
            #[inline]
            pub unsafe fn eval_toplevel(
                &mut self,
                path: *const ::std::os::raw::c_char,
                result: u32,
            ) -> bool {
                Engine_eval_toplevel(self, path, result)
            }
            #[inline]
            pub unsafe fn eval_toplevel1(
                &mut self,
                source: *mut root::JS::SourceText<root::mozilla::Utf8Unit>,
                path: *const ::std::os::raw::c_char,
                result: u32,
            ) -> bool {
                Engine_eval_toplevel1(self, source, path, result)
            }
            #[inline]
            pub unsafe fn is_preinitializing(&mut self) -> bool {
                Engine_is_preinitializing(self)
            }
            #[inline]
            pub unsafe fn toplevel_evaluated(&mut self) -> bool {
                Engine_toplevel_evaluated(self)
            }
            #[inline]
            pub unsafe fn run_event_loop(&mut self) -> bool {
                Engine_run_event_loop(self)
            }
            #[inline]
            pub unsafe fn incr_event_loop_interest(&mut self) {
                Engine_incr_event_loop_interest(self)
            }
            #[inline]
            pub unsafe fn decr_event_loop_interest(&mut self) {
                Engine_decr_event_loop_interest(self)
            }
            #[inline]
            pub unsafe fn script_value(&mut self) -> root::JS::HandleValue {
                Engine_script_value(self)
            }
            #[inline]
            pub unsafe fn has_pending_async_tasks(&mut self) -> bool {
                Engine_has_pending_async_tasks(self)
            }
            #[inline]
            pub unsafe fn queue_async_task(&mut self, task: *mut root::api::AsyncTask) {
                Engine_queue_async_task(self, task)
            }
            #[inline]
            pub unsafe fn cancel_async_task(
                &mut self,
                task: *mut root::api::AsyncTask,
            ) -> bool {
                Engine_cancel_async_task(self, task)
            }
            #[inline]
            pub unsafe fn abort(&mut self, reason: *const ::std::os::raw::c_char) {
                Engine_abort(self, reason)
            }
            #[inline]
            pub unsafe fn debug_logging_enabled(&mut self) -> bool {
                Engine_debug_logging_enabled(self)
            }
            #[inline]
            pub unsafe fn dump_value(
                &mut self,
                val: root::JS::Value,
                fp: *mut root::FILE,
            ) -> bool {
                Engine_dump_value(self, val, fp)
            }
            #[inline]
            pub unsafe fn print_stack(&mut self, fp: *mut root::FILE) -> bool {
                Engine_print_stack(self, fp)
            }
            #[inline]
            pub unsafe fn dump_pending_exception(
                &mut self,
                description: *const ::std::os::raw::c_char,
            ) {
                Engine_dump_pending_exception(self, description)
            }
            #[inline]
            pub unsafe fn dump_promise_rejection(
                &mut self,
                reason: u32,
                promise: root::JS::HandleObject,
                fp: *mut root::FILE,
            ) {
                Engine_dump_promise_rejection(self, reason, promise, fp)
            }
            #[inline]
            pub unsafe fn new() -> Self {
                let mut __bindgen_tmp = ::std::mem::MaybeUninit::uninit();
                Engine_Engine(__bindgen_tmp.as_mut_ptr());
                __bindgen_tmp.assume_init()
            }
        }
        pub type TaskCompletionCallback = ::std::option::Option<
            unsafe extern "C" fn(
                cx: *mut root::JSContext,
                receiver: root::JS::HandleObject,
            ) -> bool,
        >;
        #[repr(C)]
        pub struct AsyncTask__bindgen_vtable(::std::os::raw::c_void);
        #[repr(C)]
        #[derive(Debug, PartialEq)]
        pub struct AsyncTask {
            pub vtable_: *const AsyncTask__bindgen_vtable,
            pub handle_: root::api::PollableHandle,
        }
        extern "C" {
            #[link_name = "\u{1}_ZN3api11throw_errorEP9JSContextRK19JSErrorFormatStringPKcS6_S6_S6_"]
            pub fn throw_error(
                cx: *mut root::JSContext,
                error: *const root::JSErrorFormatString,
                arg1: *const ::std::os::raw::c_char,
                arg2: *const ::std::os::raw::c_char,
                arg3: *const ::std::os::raw::c_char,
                arg4: *const ::std::os::raw::c_char,
            ) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine2cxEv"]
            pub fn Engine_cx(this: *mut root::api::Engine) -> *mut root::JSContext;
            #[link_name = "\u{1}_ZN3api6Engine6globalEv"]
            pub fn Engine_global(this: *mut root::api::Engine) -> root::JS::HandleObject;
            /// Initialize the engine with the given filename
            #[link_name = "\u{1}_ZN3api6Engine10initializeEPKc"]
            pub fn Engine_initialize(
                this: *mut root::api::Engine,
                filename: *const ::std::os::raw::c_char,
            ) -> bool;
            /** Define a new builtin module

 The enumerable properties of the builtin object are used to construct
 a synthetic module namespace for the module.

 The enumeration and getters are called only on the first import of
 the builtin, so that lazy getters can be used to lazily initialize
 builtins.

 Once loaded, the instance is cached and reused as a singleton.*/
            #[link_name = "\u{1}_ZN3api6Engine21define_builtin_moduleEPKcN2JS6HandleINS3_5ValueEEE"]
            pub fn Engine_define_builtin_module(
                this: *mut root::api::Engine,
                id: *const ::std::os::raw::c_char,
                builtin: root::JS::HandleValue,
            ) -> bool;
            /** Treat the top-level script as a module or classic JS script.

 By default, the engine treats the top-level script as a module.
 Since not all content can be run as a module, this method allows
 changing this default, and will impact all subsequent top-level
 evaluations.*/
            #[link_name = "\u{1}_ZN3api6Engine18enable_module_modeEb"]
            pub fn Engine_enable_module_mode(this: *mut root::api::Engine, enable: bool);
            #[link_name = "\u{1}_ZN3api6Engine13eval_toplevelEPKcN2JS13MutableHandleINS3_5ValueEEE"]
            pub fn Engine_eval_toplevel(
                this: *mut root::api::Engine,
                path: *const ::std::os::raw::c_char,
                result: u32,
            ) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine13eval_toplevelERN2JS10SourceTextIN7mozilla8Utf8UnitEEEPKcNS1_13MutableHandleINS1_5ValueEEE"]
            pub fn Engine_eval_toplevel1(
                this: *mut root::api::Engine,
                source: *mut root::JS::SourceText<root::mozilla::Utf8Unit>,
                path: *const ::std::os::raw::c_char,
                result: u32,
            ) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine18is_preinitializingEv"]
            pub fn Engine_is_preinitializing(this: *mut root::api::Engine) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine18toplevel_evaluatedEv"]
            pub fn Engine_toplevel_evaluated(this: *mut root::api::Engine) -> bool;
            /** Run the async event loop as long as there's interest registered in keeping it running.

 Each turn of the event loop consists of three steps:
 1. Run reactions to all promises that have been resolves/rejected.
 2. Check if there's any interest registered in continuing to wait for async tasks, and
    terminate the loop if not.
 3. Wait for the next async tasks and execute their reactions

 Interest or loss of interest in keeping the event loop running can be signaled using the
 `Engine::incr_event_loop_interest` and `Engine::decr_event_loop_interest` methods.

 Every call to incr_event_loop_interest must be followed by an eventual call to
 decr_event_loop_interest, for the event loop to complete. Otherwise, if no async tasks remain
 pending while there's still interest in the event loop, an error will be reported.*/
            #[link_name = "\u{1}_ZN3api6Engine14run_event_loopEv"]
            pub fn Engine_run_event_loop(this: *mut root::api::Engine) -> bool;
            /// Add an event loop interest to track
            #[link_name = "\u{1}_ZN3api6Engine24incr_event_loop_interestEv"]
            pub fn Engine_incr_event_loop_interest(this: *mut root::api::Engine);
            /** Remove an event loop interest to track
 The last decrementer marks the event loop as complete to finish*/
            #[link_name = "\u{1}_ZN3api6Engine24decr_event_loop_interestEv"]
            pub fn Engine_decr_event_loop_interest(this: *mut root::api::Engine);
            /** Get the JS value associated with the top-level script execution -
 the last expression for a script, or the module namespace for a module.*/
            #[link_name = "\u{1}_ZN3api6Engine12script_valueEv"]
            pub fn Engine_script_value(
                this: *mut root::api::Engine,
            ) -> root::JS::HandleValue;
            #[link_name = "\u{1}_ZN3api6Engine23has_pending_async_tasksEv"]
            pub fn Engine_has_pending_async_tasks(this: *mut root::api::Engine) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine16queue_async_taskEPNS_9AsyncTaskE"]
            pub fn Engine_queue_async_task(
                this: *mut root::api::Engine,
                task: *mut root::api::AsyncTask,
            );
            #[link_name = "\u{1}_ZN3api6Engine17cancel_async_taskEPNS_9AsyncTaskE"]
            pub fn Engine_cancel_async_task(
                this: *mut root::api::Engine,
                task: *mut root::api::AsyncTask,
            ) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine5abortEPKc"]
            pub fn Engine_abort(
                this: *mut root::api::Engine,
                reason: *const ::std::os::raw::c_char,
            );
            #[link_name = "\u{1}_ZN3api6Engine21debug_logging_enabledEv"]
            pub fn Engine_debug_logging_enabled(this: *mut root::api::Engine) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine10dump_valueEN2JS5ValueEP8_IO_FILE"]
            pub fn Engine_dump_value(
                this: *mut root::api::Engine,
                val: root::JS::Value,
                fp: *mut root::FILE,
            ) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine11print_stackEP8_IO_FILE"]
            pub fn Engine_print_stack(
                this: *mut root::api::Engine,
                fp: *mut root::FILE,
            ) -> bool;
            #[link_name = "\u{1}_ZN3api6Engine22dump_pending_exceptionEPKc"]
            pub fn Engine_dump_pending_exception(
                this: *mut root::api::Engine,
                description: *const ::std::os::raw::c_char,
            );
            #[link_name = "\u{1}_ZN3api6Engine22dump_promise_rejectionEN2JS6HandleINS1_5ValueEEENS2_IP8JSObjectEEP8_IO_FILE"]
            pub fn Engine_dump_promise_rejection(
                this: *mut root::api::Engine,
                reason: u32,
                promise: root::JS::HandleObject,
                fp: *mut root::FILE,
            );
            #[link_name = "\u{1}_ZN3api6EngineC1Ev"]
            pub fn Engine_Engine(
                this: *mut root::api::Engine,
            ) -> *mut ::std::os::raw::c_void;
        }
    }
}
