use spin_sdk::http::{IntoResponse, Request};
use spin_sdk::http_component;

/// A simple Spin HTTP component.
#[http_component]
fn handle_js_component(req: Request) -> anyhow::Result<impl IntoResponse> {
    println!("Handling request to {:?}", req.header("spin-full-url"));
    Ok(http::Response::builder()
        .status(200)
        .header("content-type", "text/plain")
        .body("Hello, Fermyon")?)
}
