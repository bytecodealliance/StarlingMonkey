use multipart::MultipartParser;

#[derive(Debug, PartialEq)]
struct ExpectedEntry<'a> {
    name: &'a [u8],
    body: &'a [u8],
    filename: Option<&'a [u8]>,
    content_type: Option<&'a [u8]>,
}

impl<'a> ExpectedEntry<'a> {
    fn new(name: &'a [u8], body: &'a [u8]) -> Self {
        Self {
            name,
            body,
            filename: None,
            content_type: None,
        }
    }

    fn filename(mut self, filename: &'a [u8]) -> Self {
        self.filename = Some(filename);
        self
    }

    fn content_type(mut self, content_type: &'a [u8]) -> Self {
        self.content_type = Some(content_type);
        self
    }
}

pub struct TestCase<'a> {
    data: &'a [u8],
    boundary: &'a str,
    expected_entries: Vec<ExpectedEntry<'a>>,
}

impl<'a> TestCase<'a> {
    fn new(data: &'a [u8], boundary: &'a str) -> Self {
        Self {
            data,
            boundary,
            expected_entries: Vec::new(),
        }
    }

    fn expected_entry(mut self, entry: ExpectedEntry<'a>) -> Self {
        self.expected_entries.push(entry);
        self
    }

    fn run(self) {
        let mut parser = MultipartParser::new(self.data, self.boundary);
        let mut idx = 0;

        while let Some(Ok(entry)) = parser.parse_next() {
            let expected = ExpectedEntry {
                name: entry.name(),
                body: entry.value(),
                filename: entry.filename(),
                content_type: entry.content_type(),
            };

            assert_eq!(expected, self.expected_entries[idx]);
            idx += 1;
        }

        assert_eq!(
            idx,
            self.expected_entries.len(),
            "Not all expected entries were parsed"
        );
    }
}

// Some of the test inputs are copied over from multer crate, see:
// https://github.com/rwf2/multer/blob/master/tests/integration.rs

#[test]
fn test_basic() {
    let data = b"--X-BOUNDARY\r\nContent-Disposition: form-data; name=\"my_text_field\"\r\n\r\nabcd\r\n--X-BOUNDARY\r\nContent-Disposition: form-data; name=\"my_file_field\"; filename=\"a-text-file.txt\"\r\nContent-Type: text/plain\r\n\r\nHello world\nHello\r\nWorld\rAgain\r\n--X-BOUNDARY--\r\n";
    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(ExpectedEntry::new(b"my_text_field", b"abcd"))
        .expected_entry(
            ExpectedEntry::new(b"my_file_field", b"Hello world\nHello\r\nWorld\rAgain")
                .filename(b"a-text-file.txt")
                .content_type(b"text/plain"),
        )
        .run();
}

#[test]
fn test_empty() {
    let empty = b"--X-BOUNDARY--\r\n";
    TestCase::new(empty, "X-BOUNDARY").run();
}

#[test]
fn test_whitespace_boundaries() {
    let data = b"--X-BOUNDARY \t \r\nContent-Disposition: form-data; name=\"my_text_field\"\r\n\r\nabcd\r\n--X-BOUNDARY     \r\nContent-Disposition: form-data; name=\"my_file_field\"; filename=\"a-text-file.txt\"\r\nContent-Type: text/plain\r\n\r\nHello world\nHello\r\nWorld\rAgain\r\n--X-BOUNDARY--\t\t\t\t\t\r\n";

    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(ExpectedEntry::new(b"my_text_field", b"abcd"))
        .expected_entry(
            ExpectedEntry::new(b"my_file_field", b"Hello world\nHello\r\nWorld\rAgain")
                .filename(b"a-text-file.txt")
                .content_type(b"text/plain"),
        )
        .run();
}

#[test]
fn test_ignored_header() {
    let data = b"ignored header\r\n--X-BOUNDARY\r\nContent-Disposition: form-data; name=\"my_text_field\"\r\n\r\nabcd\r\n--X-BOUNDARY--\r\n";

    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(ExpectedEntry::new(b"my_text_field", b"abcd"))
        .run();
}

#[test]
fn test_ignored_header_with_leading_newline() {
    let data = b"\r\nignored header\r\n--X-BOUNDARY\r\nContent-Disposition: form-data; name=\"my_text_field\"\r\n\r\nabcd\r\n--X-BOUNDARY--\r\n";

    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(ExpectedEntry::new(b"my_text_field", b"abcd"))
        .run();
}

#[test]
fn test_leading_newline() {
    let data = b"\r\n--X-BOUNDARY\r\nContent-Disposition: form-data; name=\"my_text_field\"\r\n\r\nabcd\r\n--X-BOUNDARY--\r\n";

    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(ExpectedEntry::new(b"my_text_field", b"abcd"))
        .run();
}

#[test]
fn test_multiple_leading_newlines() {
    let data = b"\r\n\r\n--X-BOUNDARY\r\nContent-Disposition: form-data; name=\"my_text_field\"\r\n\r\nabcd\r\n--X-BOUNDARY--\r\n";

    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(ExpectedEntry::new(b"my_text_field", b"abcd"))
        .run();
}

#[test]
fn test_fuzz_simple_seed() {
    let data = include_bytes!("../fuzz/corpus/fuzz_multipart/simple.seed");

    TestCase::new(data, "X-BOUNDARY")
        .expected_entry(
            ExpectedEntry::new(b"field1", b"Joe owes =E2=82=AC100.")
                .content_type(b"text/plain;charset=UTF-8"),
        )
        .run();
}

#[test]
fn test_fuzz_multi_seed() {
    let data = include_bytes!("../fuzz/corpus/fuzz_multipart/multi.seed");

    TestCase::new(data, "--BoundaryjXo5N4HEAXWcKrw7")
        .expected_entry(ExpectedEntry::new(b"field1", b"value1"))
        .expected_entry(ExpectedEntry::new(b"field2", b"value2"))
        .expected_entry(
            ExpectedEntry::new(b"file1", b"Hello World!")
                .filename(b"dummy.txt")
                .content_type(b"foo"),
        )
        .run();
}

#[test]
fn test_malformed() {
    let data = b"--Boundary_with_capital_letters\r\nContent-Type: application/json\r\nContent-Disposition: form-data; name=\"does_this_work\"\r\n\r\nYES\r\n--Boundary_with_capital_letters-Random junk";
    let mut parser = MultipartParser::new(data, "--Boundary_with_capital_letters");

    assert!(parser.parse_next().unwrap().is_ok());
    assert!(parser.parse_next().unwrap().is_err());
}
