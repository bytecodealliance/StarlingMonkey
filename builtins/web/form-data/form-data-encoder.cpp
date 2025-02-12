#include "form-data-encoder.h"
#include "extension-api.h"
#include "form-data.h"

#include "../base64.h"
#include "../blob.h"
#include "../file.h"
#include "../streams/buf-reader.h"
#include "../streams/native-stream-source.h"

#include "encode.h"
#include "mozilla/Assertions.h"

#include <fmt/format.h>
#include <string>

namespace {

const char LF = '\n';
const char CR = '\r';
const char *CRLF = "\r\n";

size_t compute_normalized_len(std::string_view src) {
  size_t len = 0;
  const size_t newline_len = strlen(CRLF);

  for (size_t i = 0; i < src.size(); i++) {
    if (src[i] == CR) {
      if (i + 1 < src.size() && src[i + 1] == LF) {
        i++;
      }
      len += newline_len;
    } else if (src[i] == LF) {
      len += newline_len;
    } else {
      len += 1;
    }
  }

  return len;
}

// Replace every occurrence of U+000D (CR) not followed by U+000A (LF),
// and every occurrence of U+000A (LF) not preceded by U+000D (CR),
// in entry's name, by a string consisting of a U+000D (CR) and U+000A (LF).
//
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#multipart-form-data
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

// For field names and filenames for file fields, the result of the encoding must
// be escaped by replacing any 0x0A (LF) bytes with the byte sequence
// `%0A`, 0x0D (CR) with `%0D` and 0x22 (") with `%22`.
//
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#multipart-form-data
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

size_t compute_normalized_and_escaped_len(std::string_view src) {
  size_t len = 0;

  for (size_t i = 0; i < src.size(); ++i) {
    char ch = src[i];
    if (ch == '\r') {
      len += 3; // CR -> "%0D"
      len += 3; // LF -> "%0A"
      if ((i + 1) < src.size() && src[i + 1] == '\n') {
        ++i;
      }
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

std::string normalize_and_escape(std::string_view src) {
  auto normalized = normalize_newlines(src);
  MOZ_ASSERT(normalized.has_value());

  auto escaped = escape_name(normalized.value());
  MOZ_ASSERT(escaped);

  return escaped.value();
}

}// namespace

namespace builtins {
namespace web {
namespace form_data {

using blob::Blob;
using file::File;
using streams::BufReader;
using streams::NativeStreamSource;

using EntryList = JS::GCVector<FormDataEntry, 0, js::SystemAllocPolicy>;

struct StreamContext {
  StreamContext(const EntryList *entries, std::span<uint8_t> outbuf)
      : entries(entries), outbuf(outbuf), read(0), done(false) {}
  const EntryList *entries;

  std::span<uint8_t> outbuf;
  size_t read;
  bool done;

  size_t remaining() {
    MOZ_ASSERT(outbuf.size() >= read);
    return outbuf.size() - read;
  }

 // Writes as many elements from the range [first, last) into the underlying buffer as possible.
 //
 // This function is deliberately infallible as it simply writes up to the available buffer size
 // and returns how many elements were successfully written.
  template <typename I> size_t write(I first, I last) {
    auto datasz = static_cast<size_t>(std::distance(first, last));
    if (datasz == 0) {
      return 0;
    }

    size_t bufsz = remaining();
    if (bufsz == 0) {
      return 0;
    }

    size_t to_write = std::min(datasz, bufsz);
    auto dest = outbuf.begin() + read;

    std::copy_n(first, to_write, dest);
    read += to_write;
    return to_write;
  }
};

class MultipartFormDataImpl {
  enum class State : int { Start, EntryHeader, EntryBody, EntryFooter, Close, Done };

  State state_;
  std::string boundary_;
  std::string remainder_;
  std::string_view remainder_view_;

  size_t chunk_idx_;
  size_t file_leftovers_;

  bool is_draining() { return (file_leftovers_ || remainder_.size()); };

  template <typename I> void write_and_cache_remainder(StreamContext &stream, I first, I last);

  State next_state(StreamContext &stream);
  void maybe_drain_leftovers(JSContext *cx, StreamContext &stream);
  bool handle_entry_header(JSContext *cx, StreamContext &stream);
  bool handle_entry_body(JSContext *cx, StreamContext &stream);
  bool handle_entry_footer(JSContext *cx, StreamContext &stream);
  bool handle_close(JSContext *cx, StreamContext &stream);

public:
  MultipartFormDataImpl(std::string boundary)
      : state_(State::Start), boundary_(std::move(boundary)), chunk_idx_(0), file_leftovers_(0) {
    remainder_.reserve(128);
  }

  std::optional<size_t> query_length(JSContext* cx, const EntryList *entries);
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
    return State::Done;
  case State::Done:
    return State::Done;
  default:
    MOZ_ASSERT_UNREACHABLE("Invalid state");
  }
}

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
    auto blob = Blob::blob(obj);
    auto blobsz = blob->length();
    auto offset = blobsz - file_leftovers_;
    file_leftovers_ -= stream.write(blob->begin() + offset, blob->end());
  }
}

template <typename I>
void MultipartFormDataImpl::write_and_cache_remainder(StreamContext &stream, I first, I last) {
  auto datasz = static_cast<size_t>(std::distance(first, last));
  auto written = stream.write(first, last);

  MOZ_ASSERT(written <= datasz);

  auto leftover = datasz - written;
  if (leftover > 0) {
    MOZ_ASSERT(remainder_.empty());
    remainder_.assign(first + written, last);
    remainder_view_ = remainder_;
  }
}

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

    auto tmp = type.size() ? std::string_view(type) : "application/octet-stream";
    fmt::format_to(std::back_inserter(header), "; filename=\"{}\"\r\n", filename.value());
    fmt::format_to(std::back_inserter(header), "Content-Type: {}\r\n\r\n", tmp);
  }

  // If there are leftovers that didn't fit in outbuf, put it into remainder_
  // and it will be drained the next run.
  write_and_cache_remainder(stream, header.begin(), header.end());
  return true;
}

bool MultipartFormDataImpl::handle_entry_body(JSContext *cx, StreamContext &stream) {
  auto entry = stream.entries->begin()[chunk_idx_];

  if (entry.value.isString()) {
    RootedValue value_val(cx, entry.value);
    auto maybe_normalized = normalize_newlines(cx, value_val);
    if (!maybe_normalized) {
      return false;
    }

    auto normalized = maybe_normalized.value();
    write_and_cache_remainder(stream, normalized.begin(), normalized.end());
  } else {
    MOZ_ASSERT(File::is_instance(entry.value));
    RootedObject obj(cx, &entry.value.toObject());

    auto blob = Blob::blob(obj);
    auto blobsz = blob->length();
    auto written = stream.write(blob->begin(), blob->end());
    MOZ_ASSERT(written <= blobsz);
    file_leftovers_ = blobsz - written;
  }

  return true;
}

bool MultipartFormDataImpl::handle_entry_footer(JSContext *cx, StreamContext &stream) {
  auto footer = fmt::memory_buffer();
  fmt::format_to(std::back_inserter(footer), "\r\n");

  write_and_cache_remainder(stream, footer.begin(), footer.end());
  chunk_idx_ += 1;

  MOZ_ASSERT(chunk_idx_ <= stream.entries->length());
  return true;
}

bool MultipartFormDataImpl::handle_close(JSContext *cx, StreamContext &stream) {
  auto footer = fmt::memory_buffer();
  fmt::format_to(std::back_inserter(footer), "--{}--", boundary_);

  write_and_cache_remainder(stream, footer.begin(), footer.end());
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
// Returns std::nullopt if any string conversion fails.
std::optional<size_t> MultipartFormDataImpl::query_length(JSContext* cx, const EntryList *entries) {
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
      auto value = core::encode(cx, value_str);
      if (!value) {
        return std::nullopt;
      }

      total += compute_normalized_len(value);
    } else {
      MOZ_ASSERT(File::is_instance(entry.value));
      RootedObject obj(cx, &entry.value.toObject());
      RootedString filename_str(cx, File::name(obj));
      auto filename = core::encode(cx, filename_str);
      if (!filename) {
        return std::nullopt;
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
        return std::nullopt;
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

  size_t bufsz = buf.size();
  size_t total = 0;
  bool finished = false;
  RootedObject obj(cx, form_data(self));

  auto entries = FormData::entry_list(obj);
  auto impl = as_impl(self);

  // Try to fill the buffer
  while (total < bufsz && !finished) {
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
  auto impl = as_impl(self);
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

std::optional<size_t> MultipartFormData::query_length(JSContext *cx, HandleObject self) {
  RootedObject obj(cx, form_data(self));

  auto entries = FormData::entry_list(obj);
  auto impl = as_impl(self);

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
  if (auto *err = res.to_err()) {
    return nullptr;
  }

  // Hex encode bytes to string
  auto bytes = std::move(res.unwrap());
  auto bytes_str = std::string_view((char *)(bytes.ptr.get()), bytes.size());
  auto base64_str = base64::forgivingBase64Encode(bytes_str, base64::base64EncodeTable);

  auto boundary = fmt::format("--Boundary{}", base64_str);
  auto impl = new (std::nothrow) MultipartFormDataImpl(boundary);
  if (!impl) {
    return nullptr;
  }

  JS::SetReservedSlot(self, Slots::Form, JS::ObjectValue(*form_data));
  JS::SetReservedSlot(self, Slots::Inner, JS::PrivateValue(reinterpret_cast<void *>(impl)));

  return self;
}

bool MultipartFormData::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

bool MultipartFormData::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  MOZ_ASSERT_UNREACHABLE("No MultipartFormData Ctor builtin");
  return api::throw_error(cx, api::Errors::NoCtorBuiltin, class_name);
}

void MultipartFormData::finalize(JS::GCContext *gcx, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto impl = as_impl(self);
  if (impl) {
    delete impl;
  }
}

} // namespace form_data
} // namespace web
} // namespace builtins
