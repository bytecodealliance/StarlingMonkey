mod bindings;

pub use spidermonkey_rs;
pub use bindings::root::api::*;

// TODO: support configuring the bindings folder
wit_bindgen::generate!({
    world: "bindings",
    path: "../../../host-apis/wasi-0.2.3/wit",
    generate_all,
});
