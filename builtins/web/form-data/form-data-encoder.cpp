#include "form-data-encoder.h"
#include "extension-api.h"
#include "form-data.h"

#include "../base64.h"
#include "../blob.h"
#include "../file.h"
#include "../streams/buf-reader.h"
#include "../streams/native-stream-source.h"

#include "encode.h"
#include "jstypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/ResultVariant.h"

#include <fmt/format.h>
#include <optional>
#include <string>

namespace {

const char LF = '\n';
const char CR = '\r';
const char *CRLF = "\r\n";

template<typename CH> 
size_t compute_extra_characters(CH *chars, size_t len) {
  size_t extra = 0;
  for (size_t i = 0; i < len; i++) {
    auto ch = chars[i];
    if (ch == CR) {
      if (i + 1 < len) {
        char16_t next = chars[i + 1];
        if (next == LF) {
          i += 1;
          // the character is already accounted for
          continue;
        }
      }
      extra += 1;
    } else if (ch == LF) {
      extra += 1;
    }
  }
  return extra;
}

// Computes the length of a string after normalizing its newlines.
// Converts CR, LF, and CRLF into a CRLF sequence.
size_t compute_normalized_len(std::string_view src) {
  size_t len = src.size();
  len += compute_extra_characters(src.data(), len);
  return len;
}

std::optional<size_t> compute_unencoded_normalized_len(JSContext *cx, JS::HandleString value) {
  auto linear = JS_EnsureLinearString(cx, value);
  if (!linear) {
      return std::nullopt;
  }
  auto len = JS::GetDeflatedUTF8StringLength(linear);
  size_t chars_len = JS::GetLinearStringLength(linear);
  JS::AutoCheckCannotGC nogc;
  if (JS::LinearStringHasLatin1Chars(linear)) {
    auto chars = JS::GetLatin1LinearStringChars(nogc, linear);
    if (!chars) {
      return std::nullopt;
    }
    len += compute_extra_characters(chars, chars_len);
  } else {
    auto chars = JS::GetTwoByteLinearStringChars(nogc, linear);
    if (!chars) {
      return std::nullopt;
    }
    len += compute_extra_characters(chars, chars_len);
  }
  return len;
}

// Normalizes newlines in a string by replacing:
// - CR not followed by LF -> CRLF
// - LF not preceded by CR -> CRLF
std::optional<std::string> normalize_newlines(std::string_view src) {
  std::string output;

  output.reserve(compute_normalized_len(src));

  for (size_t i = 0; i < src.size(); i++) {
    if (src[i] == CR) {
      if (i + 1 < src.size() && src[i + 1] == LF) {
        i++;
      }
      output.append(CRLF);
    } else if (src[i] == LF) {
      output.append(CRLF);
    } else {
      output.push_back(src[i]);
    }
  }

  return output;
}

std::optional<std::string> normalize_newlines(JSContext *cx, HandleValue src) {
  auto chars = core::encode(cx, src);
  if (!chars) {
    return std::nullopt;
  }

  return normalize_newlines(chars);
}

// Computes the length of a string after percent encoding following characters:
// - LF (0x0A) -> "%0A"
// - CR (0x0D) -> "%0D"
// - Double quote (0x22) -> "%22"
size_t compute_escaped_len(std::string_view src) {
  size_t len = 0;
  for (char ch : src) {
    if (ch == '\n' || ch == '\r' || ch == '"') {
      len += 3;
    } else {
      ++len;
    }
  }
  return len;
}

// Percent encode following characters in a string for safe use in multipart/form-data
// field names and filenames:
// - LF (0x0A) -> "%0A"
// - CR (0x0D) -> "%0D"
// - Double quote (0x22) -> "%22"
std::optional<std::string> escape_name(std::string_view src) {
  std::string output;
  output.reserve(compute_escaped_len(src));

  for (char ch : src) {
    if (ch == '\n') {
      output.append("%0A");
    } else if (ch == '\r') {
      output.append("%0D");
    } else if (ch == '"') {
      output.append("%22");
    } else {
      output.push_back(ch);
    }
  }

  return output;
}

std::optional<std::string> escape_name(JSContext *cx, HandleValue src) {
  auto chars = core::encode(cx, src);
  if (!chars) {
    return std::nullopt;
  }

  return escape_name(chars);
}

// Computes the length of a string after both normalizing newlines and escaping characters.
size_t compute_normalized_and_escaped_len(std::string_view src) {
  size_t len = 0;

  for (size_t i = 0; i < src.size(); ++i) {
    char ch = src[i];
    if (ch == '\r') {
      if ((i + 1) < src.size() && src[i + 1] == '\n') {
        ++i;
      }
      len += 3; // CR -> "%0D"
      len += 3; // LF -> "%0A"
    } else if (ch == '\n') {
      len += 3; // CR -> "%0D"
      len += 3; // LF -> "%0A"
    } else if (ch == '"') {
      len += 3; // -> "%22"
    } else {
      len += 1;
    }
  }

  return len;
}

// Folds normalizing newlines and escaping characters in the given string into a single function.
std::string normalize_and_escape(std::string_view src) {
  auto normalized = normalize_newlines(src);
  MOZ_ASSERT(normalized.has_value());

  auto escaped = escape_name(normalized.value());
  MOZ_ASSERT(escaped);

  return escaped.value();
}

}// namespace



namespace builtins::web::form_data {

using blob::Blob;
using file::File;
using streams::BufReader;
using streams::NativeStreamSource;

using EntryList = JS::GCVector<FormDataEntry, 0, js::SystemAllocPolicy>;

struct StreamContext {
  StreamContext(const EntryList *entries, std::span<uint8_t> outbuf)
      : entries(entries), outbuf(outbuf) {}
  const EntryList *entries;

  std::span<uint8_t> outbuf;
  size_t read{0};
  bool done{false};

  [[nodiscard]] size_t remaining() const {
    MOZ_ASSERT(outbuf.size() >= read);
    return outbuf.size() - read;
  }

 // Writes as many elements from the range [first, last) into the underlying buffer as possible.
 //
 // This function is deliberately infallible as it simply writes up to the available buffer size
 // and returns how many elements were successfully written.
  template <typename I> size_t write(I first, I last) {
    auto data_size = static_cast<size_t>(std::distance(first, last));
    if (data_size == 0) {
      return 0;
    }

    size_t buf_size = remaining();
    if (buf_size == 0) {
      return 0;
    }

    size_t to_write = std::min(data_size, buf_size);
    auto dest = outbuf.begin() + read;

    std::copy_n(first, to_write, dest);
    read += to_write;
    return to_write;
  }
};

// `MultipartFormDataImpl` encodes `FormData` into a multipart/form-data body,
// following the specification in https://datatracker.ietf.org/doc/html/rfc7578.
//
// Each entry is serialized in three atomic operations: writing the header, body, and footer.
// These parts are written into a fixed-size buffer, so the implementation must handle cases
// where not all data can be written at once. Any unwritten data is stored as a "leftover"
// and will be written in the next iteration before transitioning to the next state. This
// introduces an implicit state where the encoder drains leftover data from the previous
// operation before proceeding.
//
// The algorithm is implemented as a state machine with the following states:
//   - Start:       Initialization of the process.
//   - EntryHeader: Write the boundary and header information for the current entry.
//   - EntryBody:   Write the actual content (payload) of the entry.
//   - EntryFooter: Write the trailing CRLF for the entry.
//   - Close:       Write the closing boundary indicating the end of the multipart data.
//   - Done:        Processing is complete.
class MultipartFormDataImpl {
  enum class State : uint8_t { Start, EntryHeader, EntryBody, EntryFooter, Close, Done };

  State state_{State::Start};
  std::string boundary_;
  std::string remainder_;
  std::string_view remainder_view_;

  size_t chunk_idx_{0};
  size_t file_leftovers_{0};

  bool is_draining() { return ((file_leftovers_ != 0U) || (static_cast<unsigned int>(!remainder_.empty()) != 0U)); };

  template <typename I> void write_and_store_remainder(StreamContext &stream, I first, I last);

  State next_state(StreamContext &stream);
  void maybe_drain_leftovers(JSContext *cx, StreamContext &stream);
  bool handle_entry_header(JSContext *cx, StreamContext &stream);
  bool handle_entry_body(JSContext *cx, StreamContext &stream);
  bool handle_entry_footer(JSContext *cx, StreamContext &stream);
  bool handle_close(JSContext *cx, StreamContext &stream);

public:
  MultipartFormDataImpl(std::string boundary)
      :  boundary_(std::move(boundary)) {}

  mozilla::Result<size_t, OutOfMemory> query_length(JSContext* cx, const EntryList *entries);
  std::string boundary() {  return boundary_; };
  bool read_next(JSContext *cx, StreamContext &stream);
};

MultipartFormDataImpl::State MultipartFormDataImpl::next_state(StreamContext &stream) {
  auto finished = (chunk_idx_ >= stream.entries->length());
  auto empty = stream.entries->empty();

  switch (state_) {
  case State::Start:
    return empty ? State::Done : State::EntryHeader;
  case State::EntryHeader:
    return State::EntryBody;
  case State::EntryBody:
    return State::EntryFooter;
  case State::EntryFooter:
    return finished ? State::Close : State::EntryHeader;
  case State::Close:
  case State::Done:
    // fallthrough
    return State::Done;
  default:
    MOZ_ASSERT_UNREACHABLE("Invalid state");
  }
}

// Drains any previously cached leftover data or remaining file data by writing
// it to the stream.
//
// The draining function handles two types of leftover data:
// - Metadata leftovers: This includes generated data for each entry, such as the boundary
//   delimiter, content-disposition header, etc. These are cached in `remainder_`, while
//   `remainder_view_` tracks how much remains to be written.
// - Entry value leftovers: Tracked by `file_leftovers_`, this represents the number of
//   bytes from a blob that still need to be written to the output buffer to complete
//   the entry's value.
void MultipartFormDataImpl::maybe_drain_leftovers(JSContext *cx, StreamContext &stream) {
  if (!remainder_view_.empty()) {
    auto written = stream.write(remainder_view_.begin(), remainder_view_.end());
    remainder_view_.remove_prefix(written);

    if (remainder_view_.empty()) {
      remainder_.clear();
      remainder_view_ = remainder_;
    }
  }

  if (file_leftovers_ != 0) {
    auto entry = stream.entries->begin()[chunk_idx_];
    MOZ_ASSERT(state_ == State::EntryBody);
    MOZ_ASSERT(File::is_instance(entry.value));

    RootedObject obj(cx, &entry.value.toObject());
    auto *blob = Blob::blob(obj);
    auto offset = blob->length() - file_leftovers_;
    file_leftovers_ -= stream.write(blob->begin() + offset, blob->end());
  }
}

// Writes data from the range [first, last) to the stream. If the stream cannot
// accept all the data, the unwritten part is stored in the remainder_ buffer
// for later draining.
template <typename I>
void MultipartFormDataImpl::write_and_store_remainder(StreamContext &stream, I first, I last) {
  auto to_write = static_cast<size_t>(std::distance(first, last));
  auto written = stream.write(first, last);

  MOZ_ASSERT(written <= to_write);

  auto leftover = to_write - written;
  if (leftover > 0) {
    MOZ_ASSERT(remainder_.empty());
    remainder_.assign(first + written, last);
    remainder_view_ = remainder_;
  }
}

// https://datatracker.ietf.org/doc/html/rfc7578:
// - A multipart/form-data body contains a series of parts separated by a boundary
// - The parts are delimited with a boundary delimiter, constructed using CRLF, "--",
//   and the value of the "boundary" parameter.
//   See https://datatracker.ietf.org/doc/html/rfc7578#section-4.1
// - Each part MUST contain a Content-Disposition header field where the disposition type is "form-data".
//   The Content-Disposition header field MUST also contain an additional parameter of "name"; the value of
//   the "name" parameter is the original field name from the form.
//   See https://datatracker.ietf.org/doc/html/rfc7578#section-4.2
// - For form data that represents the content of a file, a name for the file SHOULD be supplied as well,
//   by using a "filename" parameter of the Content-Disposition header field.
//   See https://datatracker.ietf.org/doc/html/rfc7578#section-4.2
// - Each part MAY have an (optional) "Content-Type" header field, which defaults to "text/plain".  If the
//   contents of a file are to be sent, the file data SHOULD be labeled with an appropriate media type, if
//   known, or "application/octet-stream".
//
// Additionaly from the https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#multipart%2Fform-data-encoding-algorithm
// - The parts of the generated multipart/form-data resource that correspond to non-file fields
//   must not have a `Content-Type` header specified.
// - Replace every occurrence of U+000D (CR) not followed by U+000A (LF), and every occurrence
//   of U+000A (LF) not preceded by U+000D (CR), in entry's name, by a string consisting of a
//   U+000D (CR) and U+000A (LF)
// - For field names and filenames for file fields, the result of the encoding in the previous
//   bullet point must be escaped by replacing any 0x0A (LF) bytes with the byte sequence `%0A`,
//   0x0D (CR) with `%0D` and 0x22 (") with `%22`.
//
// The two bullets above for "name" are folded into `normalize_and_escape`. The filename on the other
// hand is escaped using `escape_name`.
bool MultipartFormDataImpl::handle_entry_header(JSContext *cx, StreamContext &stream) {
  auto entry = stream.entries->begin()[chunk_idx_];
  auto header = fmt::memory_buffer();
  auto name = normalize_and_escape(entry.name);

  fmt::format_to(std::back_inserter(header), "--{}\r\n", boundary_);
  fmt::format_to(std::back_inserter(header), "Content-Disposition: form-data; name=\"{}\"", name);

  if (entry.value.isString()) {
    fmt::format_to(std::back_inserter(header), "\r\n\r\n");
  } else {
    MOZ_ASSERT(File::is_instance(entry.value));
    RootedObject obj(cx, &entry.value.toObject());

    RootedValue filename_val(cx, JS::StringValue(File::name(obj)));
    auto filename = escape_name(cx, filename_val);
    if (!filename) {
      return false;
    }

    RootedString type_str(cx, Blob::type(obj));
    auto type = core::encode(cx, type_str);
    if (!type) {
      return false;
    }

    auto tmp = (type.size() != 0U) ? std::string_view(type) : "application/octet-stream";
    fmt::format_to(std::back_inserter(header), "; filename=\"{}\"\r\n", filename.value());
    fmt::format_to(std::back_inserter(header), "Content-Type: {}\r\n\r\n", tmp);
  }

  // If there are leftovers that didn't fit in outbuf, put it into remainder_
  // and it will be drained the next run.
  write_and_store_remainder(stream, header.begin(), header.end());
  return true;
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#multipart%2Fform-data-encoding-algorithm
// - If entry's value is not a File object, then replace every occurrence of U+000D (CR) not followed by U+000A (LF),
//   and every occurrence of U+000A (LF) not preceded by U+000D (CR), in entry's value, by a string consisting of a
//   U+000D (CR) and U+000A (LF) - this is folded into `normalize_newlines`.
bool MultipartFormDataImpl::handle_entry_body(JSContext *cx, StreamContext &stream) {
  auto entry = stream.entries->begin()[chunk_idx_];

  if (entry.value.isString()) {
    RootedValue value_val(cx, entry.value);
    auto maybe_normalized = normalize_newlines(cx, value_val);
    if (!maybe_normalized) {
      return false;
    }

    auto normalized = maybe_normalized.value();
    write_and_store_remainder(stream, normalized.begin(), normalized.end());
  } else {
    MOZ_ASSERT(File::is_instance(entry.value));
    RootedObject obj(cx, &entry.value.toObject());

    auto *blob = Blob::blob(obj);
    auto to_write = blob->length();
    auto written = stream.write(blob->begin(), blob->end());
    MOZ_ASSERT(written <= to_write);
    file_leftovers_ = to_write - written;
  }

  return true;
}

// https://datatracker.ietf.org/doc/html/rfc2046#section-5.1.1 - writes `crlf`
bool MultipartFormDataImpl::handle_entry_footer(JSContext *cx, StreamContext &stream) {
  auto footer = fmt::memory_buffer();
  fmt::format_to(std::back_inserter(footer), "\r\n");

  write_and_store_remainder(stream, footer.begin(), footer.end());
  chunk_idx_ += 1;

  MOZ_ASSERT(chunk_idx_ <= stream.entries->length());
  return true;
}

// https://datatracker.ietf.org/doc/html/rfc2046#section-5.1.1
//
// The boundary delimiter line following the last body part is a distinguished delimiter that
// indicates that no further body parts will follow.  Such a delimiter line is identical to
// the previous delimiter lines, with the addition of two more hyphens after the boundary
// parameter value.
bool MultipartFormDataImpl::handle_close(JSContext *cx, StreamContext &stream) {
  auto footer = fmt::memory_buffer();
  fmt::format_to(std::back_inserter(footer), "--{}--", boundary_);

  write_and_store_remainder(stream, footer.begin(), footer.end());
  return true;
}

bool MultipartFormDataImpl::read_next(JSContext *cx, StreamContext &stream) {
  maybe_drain_leftovers(cx, stream);
  if (is_draining()) {
    return true;
  }

  state_ = next_state(stream);

  switch (state_) {
  case State::EntryHeader: {
    return handle_entry_header(cx, stream);
  }
  case State::EntryBody: {
    return handle_entry_body(cx, stream);
  }
  case State::EntryFooter: {
    return handle_entry_footer(cx, stream);
  }
  case State::Close: {
    return handle_close(cx, stream);
  }
  case State::Done: {
    stream.done = true;
    return true;
  }
  default:
    MOZ_ASSERT_UNREACHABLE("Invalid state");
    return false;
  }
}

// Computes the total size (in bytes) of the encoded multipart/form-data stream.
//
// Returns `std::nullopt` if any string conversion fails. This function simulates
// the multipart/form-data encoding process without actually writing to a buffer.
// Instead, it accumulates the total size of each encoding step.
mozilla::Result<size_t, OutOfMemory> MultipartFormDataImpl::query_length(JSContext* cx, const EntryList *entries) {
  size_t total = 0;

  constexpr const char* content_disp_lit = "Content-Disposition: form-data; name=\"\"";
  constexpr const char* content_type_lit = "Content-Type: ";
  constexpr const char* filename_lit = "; filename=\"\"";
  constexpr const char* default_mime_lit = "application/octet-stream";

  const size_t content_disp_len = strlen(content_disp_lit);
  const size_t content_type_len = strlen(content_type_lit);
  const size_t filename_len = strlen(filename_lit);
  const size_t default_mime_len = strlen(default_mime_lit);
  const size_t crlf_len = strlen(CRLF);

  // For every entry in the FormData
  for (const auto& entry : *entries) {
    // Add: "--" + boundary + CRLF
    total += 2 + boundary_.size() + crlf_len;

    // Add: "Content-Disposition: form-data; name=\"\""
    total += content_disp_len;
    total += compute_normalized_and_escaped_len(entry.name);

    if (entry.value.isString()) {
      // Terminate the header
      total += 2 * crlf_len;

      RootedValue value_str(cx, entry.value);
      JS::RootedString value(cx, JS::ToString(cx, value_str));
      if (!value) {
        return mozilla::Result<size_t, OutOfMemory>(OutOfMemory {});
      }
      auto value_len = compute_unencoded_normalized_len(cx, value);
      if (!value_len.has_value()) {
        return mozilla::Result<size_t, OutOfMemory>(OutOfMemory {});
      }
      total += value_len.value();
    } else {
      MOZ_ASSERT(File::is_instance(entry.value));
      RootedObject obj(cx, &entry.value.toObject());
      RootedString filename_str(cx, File::name(obj));
      auto filename = core::encode(cx, filename_str);
      if (!filename) {
        return mozilla::Result<size_t, OutOfMemory>(OutOfMemory {});
      }

      // Literal: ; filename=""
      total += filename_len;
      total += compute_escaped_len(filename);
      total += crlf_len;

      // Literal: "Content-Type: "
      total += content_type_len;

      // The type string (defaulting to "application/octet-stream" if empty)
      RootedString type_str(cx, Blob::type(obj));
      auto type = core::encode(cx, type_str);
      if (!type) {
        return mozilla::Result<size_t, OutOfMemory>(OutOfMemory {});
      }

      total += type.size() > 0 ? type.size() : default_mime_len;

      // Terminate the header
      total += 2 * crlf_len;

      // Add payload
      total += Blob::blob_size(obj);
    }

    // Each entry is terminated with a CRLF.
    total += crlf_len;
  }

  // This is written as: "--" + boundary + "--"
  total += 2 + boundary_.size() + 2;

  return total;
}

const JSFunctionSpec MultipartFormData::static_methods[] = {JS_FS_END};
const JSPropertySpec MultipartFormData::static_properties[] = {JS_PS_END};
const JSFunctionSpec MultipartFormData::methods[] = {JS_FS_END};
const JSPropertySpec MultipartFormData::properties[] = {JS_PS_END};

bool MultipartFormData::read(JSContext *cx, HandleObject self, std::span<uint8_t> buf, size_t start,
                             size_t *read, bool *done) {
  MOZ_ASSERT(is_instance(self));

  if (buf.empty()) {
    *read = 0;
    return true;
  }

  size_t buffer_size = buf.size();
  size_t total = 0;
  bool finished = false;
  RootedObject obj(cx, form_data(self));

  auto *entries = FormData::entry_list(obj);
  auto *impl = as_impl(self);

  // Try to fill the buffer
  while (total < buffer_size && !finished) {
    auto subspan = buf.subspan(total);
    auto stream = StreamContext(entries, subspan);

    if (!impl->read_next(cx, stream)) {
      return false;
    }

    total += stream.read;
    finished = stream.done;
  }

  // Delay reporting done to produce a separate empty chunk.
  *done = finished && total == 0;
  *read = total;
  return true;
}

std::string MultipartFormData::boundary(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto *impl = as_impl(self);
  MOZ_ASSERT(impl);

  return impl->boundary();
}

MultipartFormDataImpl *MultipartFormData::as_impl(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return reinterpret_cast<MultipartFormDataImpl *>(
      JS::GetReservedSlot(self, Slots::Inner).toPrivate());
}

JSObject *MultipartFormData::form_data(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  return &JS::GetReservedSlot(self, Slots::Form).toObject();
}

mozilla::Result<size_t, OutOfMemory> MultipartFormData::query_length(JSContext *cx, HandleObject self) {
  RootedObject obj(cx, form_data(self));

  auto *entries = FormData::entry_list(obj);
  auto *impl = as_impl(self);

  return impl->query_length(cx, entries);
}

JSObject *MultipartFormData::encode_stream(JSContext *cx, HandleObject self) {
  RootedObject reader(cx, BufReader::create(cx, self, read));
  if (!reader) {
    return nullptr;
  }

  RootedObject native_stream(cx, BufReader::stream(reader));
  RootedObject default_stream(cx, NativeStreamSource::stream(native_stream));

  return default_stream;
}

JSObject *MultipartFormData::create(JSContext *cx, HandleObject form_data) {
  JS::RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self) {
    return nullptr;
  }

  if (!FormData::is_instance(form_data)) {
    return nullptr;
  }

  auto res = host_api::Random::get_bytes(12);
  if (res.to_err()) {
    return nullptr;
  }

  // The requirements for boundary are (https://datatracker.ietf.org/doc/html/rfc2046#section-5.1.1):
  // Boundary delimiters must not appear within the encapsulated material, and must be no longer than
  // 70 characters, not counting the two leading hyphens and consist of bcharsnospace characters,
  // where EBNF for bcharsnospace is as follows:
  //
  // bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" / "+" / "_" / "," / "-" / "." / "/" / ":" / "=" / "?"
  //
  // e.g.:
  // This implementation: --BoundaryjXo5N4HEAXWcKrw7
  // WebKit: ----WebKitFormBoundaryhpShnP1JqrBTVTnC
  // Gecko:  ----geckoformboundary8c79e61efa53dc5d441481912ad86113
  auto bytes = std::move(res.unwrap());
  auto bytes_str = std::string_view((char *)(bytes.ptr.get()), bytes.size());
  auto base64_str = base64::forgivingBase64Encode(bytes_str, base64::base64EncodeTable);

  auto boundary = fmt::format("--StarlingMonkeyFormBoundary{}", base64_str);
  auto impl = js::MakeUnique<MultipartFormDataImpl>(boundary);
  if (!impl) {
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::Form, JS::ObjectValue(*form_data));
  JS::SetReservedSlot(self, Slots::Inner, JS::PrivateValue(reinterpret_cast<void *>(impl.release())));

  return self;
}

bool MultipartFormData::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

bool MultipartFormData::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  return api::throw_error(cx, api::Errors::NoCtorBuiltin, class_name);
}

void MultipartFormData::finalize(JS::GCContext *gcx, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto *impl = as_impl(self);
  if (impl) {
    js_delete(impl);
  }
}

} // namespace builtins::web::form_data


