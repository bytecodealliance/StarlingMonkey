use winnow::ascii::{crlf, Caseless};
use winnow::combinator::{alt, cut_err, empty, fail, opt, preceded};
use winnow::combinator::{repeat_till, separated, seq, terminated};
use winnow::error::{ErrMode, FromExternalError, ModalResult, StrContext};
use winnow::prelude::*;
use winnow::token::{take_until, take_while};

use crate::{Entry, EntryInfo, Stream};

use crate::error::Error;
use crate::trivia::{horizontal_and_vertical_space, is_token};
use crate::trivia::{is_visible_ascii, quoted_string, take_till_crlf, trim, ws};

#[derive(Copy, Clone, Debug)]
enum Field<'a> {
    /// form-data field.
    FormData,
    /// The `name` attribute in a `Content-Disposition` header.
    Name(&'a [u8]),
    /// The `filename` attribute in a `Content-Disposition` header.
    Filename(&'a [u8]),
    /// Any other unrecognized or unsupported field.
    Other,
}

#[derive(Copy, Clone, Debug)]
enum Header<'a> {
    /// A `Content-Disposition` header, possibly with a filename.
    ContentDisposition {
        name: &'a [u8],
        filename: Option<&'a [u8]>,
    },
    /// A `Content-Type` header.
    ContentType(&'a [u8]),
    /// Any other unrecognized header.
    Other,
}

// Adopted from gecko MultipartParser:
// https://searchfox.org/mozilla-central/source/dom/base/BodyUtil.cpp#67

pub(crate) fn next_entry<'s>(input: &mut Stream<'s>) -> ModalResult<Option<Entry<'s>>> {
    // Read over a boundary and advance stream to the position after the end of it.
    let boundary = boundary_from_stream(input);
    (
        horizontal_and_vertical_space,
        take_until(0.., boundary),
        boundary,
    )
        .parse_next(input)?;

    // Check for end of data, which is boundary followed by two dashes: X-BOUNDARY--.
    if opt("--").parse_next(input)?.is_some() {
        return Ok(None);
    };

    // Position the input at the beginning of the headers (after the CRLF). Allow horizontal spaces before.
    (ws, crlf).parse_next(input)?;

    Ok(Some(
        seq!(Entry {
            info: entry_info,
            value: value_body,
        })
        .parse_next(input)?,
    ))
}

fn value_body<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    let boundary = boundary_from_stream(input);
    let body = take_until(1.., boundary).parse_next(input)?;

    let mut end = body.len();

    // Strip optional `--`
    if end >= 2 && body[end - 1] == b'-' && body[end - 2] == b'-' {
        end -= 2;
    }

    // Ensure there's a trailing CRLF and remove it
    if end < 2 || body[end - 1] != b'\n' || body[end - 2] != b'\r' {
        return Err(ErrMode::from_external_error(input, Error::InvalidBoundary));
    }

    end -= 2;

    // body[..end] is everything up to but not including
    // the optional `--` and the mandatory CRLF.
    Ok(&body[..end])
}

pub(crate) fn entry_info<'s>(input: &mut Stream<'s>) -> ModalResult<EntryInfo<'s>> {
    let headers: Vec<_> = repeat_till(1.., terminated(header, crlf), (ws, crlf))
        .parse_next(input)?
        .0;

    let (name, filename, content_type) =
        headers
            .into_iter()
            .fold((None, None, None), |(mut nm, mut fnm, mut ty), item| {
                match item {
                    Header::ContentDisposition { name, filename } => {
                        nm = Some(name);
                        fnm = filename;
                    }
                    Header::ContentType(name) => {
                        ty = Some(name);
                    }
                    Header::Other => (),
                }
                (nm, fnm, ty)
            });

    name.map(|name| EntryInfo {
        name,
        filename,
        content_type,
    })
    .ok_or(ErrMode::from_external_error(
        input,
        Error::MissingContentDisposition,
    ))
}

// https://datatracker.ietf.org/doc/html/rfc7230#section-3.2.4
//
// No whitespace is allowed between the header field-name and colon.
#[rustfmt::skip]
fn header<'s>(input: &mut Stream<'s>) -> ModalResult<Header<'s>> {
    alt((
        preceded(
            (ws, Caseless("content-disposition:")),
            cut_err(content_disposition),
        ),
        preceded(
            (ws, Caseless("content-type:")),
            cut_err(content_type)
        ),
        cut_err((take_while(1.., is_token), (':', ws), take_till_crlf).value(Header::Other)),
    ))
    .parse_next(input)
}

fn take_value<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    alt((
        terminated(quoted_string, ws),
        take_while(1.., |c| is_visible_ascii(c) && c != b';'),
        fail.context(StrContext::Label("header value")),
    ))
    .map(trim)
    .parse_next(input)
}

fn field<'s>(input: &mut Stream<'s>) -> ModalResult<Field<'s>> {
    alt((
        preceded(ws, "form-data").map(|_| Field::FormData),
        preceded((ws, "name", ws, '=', ws), take_value).map(Field::Name),
        preceded((ws, "filename", ws, '=', ws), take_value).map(Field::Filename),
        empty.value(Field::Other),
    ))
    .parse_next(input)
}

fn content_disposition<'s>(input: &mut Stream<'s>) -> ModalResult<Header<'s>> {
    let mut name = None;
    let mut filename = None;
    let mut seen_form_data = false;

    let fields: Vec<_> = separated(1.., field, (ws, ';', ws)).parse_next(input)?;

    for field in fields {
        match field {
            Field::FormData => seen_form_data = true,
            Field::Name(n) if seen_form_data => name = Some(n),
            Field::Filename(f) if seen_form_data => filename = Some(f),
            _ => (),
        }
    }

    name.map(|name| Header::ContentDisposition { name, filename })
        .ok_or(ErrMode::from_external_error(input, Error::MissingName))
}

fn content_type<'s>(input: &mut Stream<'s>) -> ModalResult<Header<'s>> {
    take_while(1.., is_visible_ascii)
        .map(trim)
        .map(Header::ContentType)
        .parse_next(input)
}

fn boundary_from_stream<'s>(input: &Stream<'s>) -> &'s [u8] {
    input.state.0
}

pub(crate) fn boundary_from_content_type<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    (ws, "multipart/form-data", ws, ';').parse_next(input)?;
    take_until(0.., "boundary").parse_next(input)?;

    preceded(("boundary", ws, '=', ws), take_while(1.., is_visible_ascii))
        .map(trim)
        .parse_next(input)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug)]
    struct TestCase<'a> {
        input: &'a [u8],
        expected_name: Option<&'a [u8]>,
        expected_filename: Option<&'a [u8]>,
        expected_content_type: Option<&'a [u8]>,
        expect_error: bool,
    }

    impl<'a> TestCase<'a> {
        /// Create a new test case with the given input.
        fn new(input: &'a [u8]) -> Self {
            Self {
                input,
                expected_name: None,
                expected_filename: None,
                expected_content_type: None,
                expect_error: false,
            }
        }

        /// Set the expected `name` value.
        #[must_use]
        fn expected_name(mut self, name: &'a [u8]) -> Self {
            self.expected_name = Some(name);
            self
        }

        /// Set the expected `filename` value.
        #[must_use]
        fn expected_filename(mut self, filename: &'a [u8]) -> Self {
            self.expected_filename = Some(filename);
            self
        }

        /// Set the expected `content_type` value.
        #[must_use]
        fn expected_content_type(mut self, content_type: &'a [u8]) -> Self {
            self.expected_content_type = Some(content_type);
            self
        }

        /// Mark this test case as expecting a parse error.
        #[must_use]
        fn expect_error(mut self) -> Self {
            self.expect_error = true;
            self
        }

        /// Run the test case.
        fn run(self) {
            let mut stream = winnow::Stateful {
                input: self.input,
                ..Default::default()
            };
            let result = entry_info(&mut stream);

            match self.expect_error {
                true => assert!(result.is_err(),),
                false => {
                    let info = result.expect("Parsing should succeed");
                    assert_eq!(info.name, self.expected_name.expect("Name is set"));
                    assert_eq!(info.filename, self.expected_filename);
                    assert_eq!(info.content_type, self.expected_content_type);
                }
            }
        }
    }

    #[test]
    fn test_valid_headers_with_filename() {
        TestCase::new(b"Content-Disposition: form-data; name=foo; filename=dummy.txt\r\nContent-Type: text/plain\r\n\r\n")
        .expected_name(b"foo")
        .expected_filename(b"dummy.txt")
        .expected_content_type(b"text/plain")
        .run();
    }

    #[test]
    fn test_valid_headers_without_filename() {
        TestCase::new(b"Content-Disposition: form-data; name=foo\r\n\r\n")
            .expected_name(b"foo")
            .run();
    }

    #[test]
    fn test_extra_whitespace_and_case_insensitivity() {
        TestCase::new(b"  CoNtEnT-DiSpOsItIoN:   form-data  ;   name  =   foo  ; filename  =   dummy.txt  \r\n content-type:  text/plain \r\n  \r\n")
        .expected_name(b"foo")
        .expected_filename(b"dummy.txt")
        .expected_content_type(b"text/plain")
        .run();
    }

    #[test]
    fn test_multiple_headers_last_one_wins() {
        TestCase::new(b"Content-Disposition: form-data; name=first\r\nContent-Disposition: form-data; name=second; filename=second.txt\r\nContent-Type: text/plain\r\n\r\n")
        .expected_name(b"second")
        .expected_filename(b"second.txt")
        .expected_content_type(b"text/plain")
        .run();
    }

    #[test]
    fn test_ignore_unknown_headers_among_known_headers() {
        TestCase::new(b"Content-Disposition: form-data; name=foo\r\nX-Custom: ignoreme\r\nContent-Type: text/plain\r\n\r\n")
        .expected_name(b"foo")
        .expected_content_type(b"text/plain")
        .run();
    }

    #[test]
    fn test_ignore_multiple_unknown_headers() {
        TestCase::new(b"X-Ignored: value1\r\nContent-Disposition: form-data; name=foo; filename=dummy.txt\r\nX-Extra: value2\r\nContent-Type: text/plain\r\nX-Another: value3\r\n\r\n")
        .expected_name(b"foo")
        .expected_filename(b"dummy.txt")
        .expected_content_type(b"text/plain")
        .run();
    }

    #[test]
    fn test_only_unknown_headers_results_in_error() {
        TestCase::new(b"X-Unknown: foo\r\nAnother-Header: bar\r\n")
            .expect_error()
            .run();
    }

    #[test]
    fn test_missing_content_disposition() {
        TestCase::new(b"Content-Type: text/plain\r\n\r\n")
            .expect_error()
            .run();
    }

    #[test]
    fn test_missing_name_in_content_disposition() {
        TestCase::new(b"Content-Disposition: form-data; filename=dummy.txt\r\n\r\n")
            .expect_error()
            .run();
    }

    #[test]
    fn test_invalid_header_token() {
        TestCase::new(
            b"Content-Disposition: form-data; name=foo\r\n\
              invalid-header-name() : value\r\n\r\n",
        )
        .expect_error()
        .run();
    }
}
