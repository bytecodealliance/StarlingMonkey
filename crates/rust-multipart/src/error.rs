use std::fmt::Display;

#[derive(Copy, Clone, Debug)]
pub(crate) enum Error {
    MissingName,
    InvalidBoundary,
    MissingContentDisposition,
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::MissingName => {
                write!(f, "name field is missing from content-disposition header")
            }
            Error::InvalidBoundary => {
                write!(f, "Invalid boundary position, CRLF not found")
            }
            Error::MissingContentDisposition => {
                write!(f, "content-disposition is missing from headers")
            }
        }
    }
}

impl std::error::Error for Error {
    fn description(&self) -> &'static str {
        "Multipart parse error"
    }
}
