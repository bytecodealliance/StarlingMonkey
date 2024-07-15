mod bindings;

pub use spidermonkey_rs;
pub use bindings::root::api::*;

wit_bindgen::generate!({
    world: "bindings",
    path: "../../../host-apis/wasi-0.2.0/wit",
    generate_all,
});
