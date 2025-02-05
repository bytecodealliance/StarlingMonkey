use winnow::error::ModalResult;
use winnow::prelude::*;
use winnow::Stateful;

mod error;
mod parser;
mod trivia;

#[cfg(feature = "capi")]
pub mod capi;

type Stream<'s> = Stateful<&'s [u8], Boundary<'s>>;

/// Represents the boundary used to separate parts in multipart form data.
#[derive(Debug, Copy, Clone, Default, Eq, PartialEq)]
struct Boundary<'b>(&'b [u8]);

impl<'b> From<&'b [u8]> for Boundary<'b> {
    fn from(b: &'b [u8]) -> Self {
        Boundary(b)
    }
}

/// Holds metadata for a single multipart form data entry.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
struct EntryInfo<'a> {
    /// The `name` attribute from `Content-Disposition`.
    name: &'a [u8],
    /// The optional `filename` attribute from `Content-Disposition`.
    filename: Option<&'a [u8]>,
    /// The optional `Content-Type`.
    content_type: Option<&'a [u8]>,
}

/// Represents a single multipart form data entry, including its metadata and value.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct Entry<'a> {
    /// Metadata describing this form data entry.
    info: EntryInfo<'a>,
    /// The raw bytes of the entry's content.
    value: &'a [u8],
}

impl Entry<'_> {
    /// Returns the `name` attribute from the `Content-Disposition` header.
    pub fn name(&self) -> &[u8] {
        self.info.name
    }

    /// Returns the optional `filename` attribute from the `Content-Disposition` header.
    pub fn filename(&self) -> Option<&[u8]> {
        self.info.filename
    }

    /// Returns the optional `Content-Type` header value.
    pub fn content_type(&self) -> Option<&[u8]> {
        self.info.content_type
    }

    /// Returns the raw bytes of the entry's content.
    pub fn value(&self) -> &[u8] {
        self.value
    }
}

#[derive(Debug, Clone)]
pub struct MultipartParser<'a> {
    stream: Stream<'a>,
}

impl<'a> MultipartParser<'a> {
    pub fn new(data: &'a [u8], boundary: &'a str) -> Self {
        let stream = Stream {
            input: data,
            state: boundary.as_bytes().into(),
        };

        Self { stream }
    }

    pub fn parse_next(&mut self) -> Option<ModalResult<Entry<'_>>> {
        parser::next_entry.parse_next(&mut self.stream).transpose()
    }
}

pub fn boundary_from_content_type(input: &[u8]) -> Option<&[u8]> {
    let mut stream = Stream {
        input,
        ..Default::default()
    };

    parser::boundary_from_content_type(&mut stream).ok()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_boundary() {
        let content_type = b"multipart/form-data; boundary=X-BOUNDARY";
        assert_eq!(
            boundary_from_content_type(content_type),
            Some(b"X-BOUNDARY".as_slice())
        );

        let content_type = b"multipart/form-data; boundary=\"--X-BOUNDARY\"";
        assert_eq!(
            boundary_from_content_type(content_type),
            Some(b"--X-BOUNDARY".as_slice())
        );

        let content_type = b"multipart/form-data; boundary=------X-BOUNDARY";
        assert_eq!(
            boundary_from_content_type(content_type),
            Some(b"------X-BOUNDARY".as_slice())
        );

        let content_type = b"boundary=------X-BOUNDARY";
        assert!(boundary_from_content_type(content_type).is_none());
    }
}
