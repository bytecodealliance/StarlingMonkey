use winnow::combinator::delimited;
use winnow::prelude::*;
use winnow::token::{take_until, take_while};
use winnow::ModalResult;

use crate::Stream;

pub(crate) fn is_visible_ascii(b: u8) -> bool {
    b >= 32 && b != 127 || b == b'\t'
}

pub(crate) fn is_horizontal_space(c: u8) -> bool {
    c == b' ' || c == b'\t'
}

pub(crate) fn is_whitespace(c: u8) -> bool {
    c == b' ' || c == b'\t' || c == b'\n' || c == b'\r'
}

pub(crate) fn ws<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    take_while(0.., is_horizontal_space).parse_next(input)
}

pub(crate) fn horizontal_and_vertical_space<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    take_while(0.., is_whitespace).parse_next(input)
}

pub(crate) fn take_till_crlf<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    take_until(1.., "\r\n").parse_next(input)
}

// From section 2.2 of RFC 2616
#[allow(clippy::match_same_arms)]
#[allow(clippy::match_like_matches_macro)]
pub(crate) fn is_token(c: u8) -> bool {
    match c {
        128..=255 => false,
        0..=31 => false,
        b'(' => false,
        b')' => false,
        b'<' => false,
        b'>' => false,
        b'@' => false,
        b',' => false,
        b';' => false,
        b':' => false,
        b'\\' => false,
        b'"' => false,
        b'/' => false,
        b'[' => false,
        b']' => false,
        b'?' => false,
        b'=' => false,
        b'{' => false,
        b'}' => false,
        b' ' => false,
        _ => true,
    }
}

pub(crate) fn quoted_string<'s>(input: &mut Stream<'s>) -> ModalResult<&'s [u8]> {
    delimited(
        '"',
        take_while(0.., |c| c != b'"' && c != b'\r' && c != b'\n' && c != b'\0'),
        '"',
    )
    .parse_next(input)
}

pub(crate) fn trim(input: &[u8]) -> &[u8] {
    let set = [b' ', b'\\', b'"'];
    let mut start = 0;
    let mut end = input.len();

    while start < end && set.contains(&input[start]) {
        start += 1;
    }

    while end > start && set.contains(&input[end - 1]) {
        end -= 1;
    }

    &input[start..end]
}
