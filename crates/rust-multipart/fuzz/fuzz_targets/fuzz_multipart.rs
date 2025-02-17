#![no_main]

use libfuzzer_sys::fuzz_target;
use multipart::MultipartParser;

fuzz_target!(|data: &[u8]| {
    let mut parser = MultipartParser::new(data, "X-BOUNDARY");
    let mut retries = 0;

    while retries < 5 {
        let entry = parser.parse_next();
        match entry {
            Some(Ok(_)) => continue,
            Some(Err(_)) | None => retries += 1,
        };
    }
});
