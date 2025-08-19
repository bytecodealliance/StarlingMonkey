#include "form-data-parser.h"
#include "builtin.h"
#include "decode.h"
#include "form-data.h"
#include "rust-encoding.h"
#include "rust-multipart-ffi.h"
#include "rust-url.h"

#include "../file.h"

namespace {

JSString *to_owned_string(JSContext *cx, jsmultipart::Slice src) {
  const auto *chars = reinterpret_cast<const char *>(src.data);

  std::string_view sv(chars, src.len);
  return core::decode(cx, sv);
}

JSObject *to_owned_buffer(JSContext *cx, jsmultipart::Slice src) {
  auto buf = mozilla::MakeUnique<uint8_t[]>(src.len);
  if (!buf) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  std::copy_n(src.data, src.len, buf.get());

  JS::RootedObject buffer(cx, JS::NewArrayBufferWithContents(
      cx, src.len, buf.get(), JS::NewArrayBufferOutOfMemory::CallerMustFreeMemory));
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  // `array_buffer` now owns `buf`
  std::ignore = (buf.release());

  JS::RootedObject byte_array(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, src.len));
  if (!byte_array) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  return byte_array;
}

} // namespace



namespace builtins::web::form_data {

using file::File;
using form_data::FormData;
using jsmultipart::RetCode;

class MultipartParser : public FormDataParser {
  std::string boundary_;

public:
  MultipartParser(std::string_view boundary) : boundary_(boundary) {}

  JSObject *parse(JSContext *cx, std::string_view body) override;
};

JSObject *MultipartParser::parse(JSContext *cx, std::string_view body) {
  RootedObject formdata(cx, FormData::create(cx));
  if (!formdata) {
    return nullptr;
  }

  if (body.empty()) {
    return formdata;
  }

  auto done = false;
  const auto *data = reinterpret_cast<const uint8_t *>(body.data());

  jsmultipart::Slice input{.data=data, .len=body.size()};
  jsmultipart::Entry entry{};

  auto *encoding = const_cast<jsencoding::Encoding *>(jsencoding::encoding_for_label_no_replacement(
      reinterpret_cast<uint8_t *>(const_cast<char *>("UTF-8")), 5));

  auto deleter1 = [&](auto *state) { jsmultipart::multipart_parser_free(state); };
  std::unique_ptr<jsmultipart::State, decltype(deleter1)> parser(
      jsmultipart::multipart_parser_new(&input, boundary_.c_str()), deleter1);

  if (!parser) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  auto deleter2 = [&](auto *dec) { jsencoding::decoder_free(dec); };
  std::unique_ptr<jsencoding::Decoder, decltype(deleter2)> decoder(
      jsencoding::encoding_new_decoder_with_bom_removal(encoding), deleter2);

  if (!decoder) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  while (!done) {
    auto ret = jsmultipart::multipart_parser_next(parser.get(), &entry);

    switch (ret) {
    case RetCode::Error: {
      return nullptr;
    }
    case RetCode::Eos: {
      done = true;
      break;
    }
    case RetCode::Ok: {
      MOZ_ASSERT(entry.name.data != nullptr);
      MOZ_ASSERT(entry.value.data != nullptr);

      std::string_view name((char *)entry.name.data, entry.name.len);

      /// https://fetch.spec.whatwg.org/#body-mixin
      if (entry.filename.data == nullptr) {
        // Each part whose `Content-Disposition` header does not contain a `filename`
        // parameter must be parsed into an entry whose value is the UTF-8 decoded without
        // BOM content of the part.
        auto src_size = entry.value.len;
        auto dst_size = jsencoding::decoder_max_utf16_buffer_length(decoder.get(), src_size);

        JS::UniqueTwoByteChars data(new char16_t[dst_size + 1]);
        if (!data) {
          JS_ReportOutOfMemory(cx);
          return nullptr;
        }

        bool ignore = false;
        auto *dst = reinterpret_cast<uint16_t *>(data.get());
        const auto *src = entry.value.data;

        jsencoding::decoder_decode_to_utf16(decoder.get(), src, &src_size, dst, &dst_size, false, &ignore);

        JS::RootedString value(cx, JS_NewUCString(cx, std::move(data), dst_size));
        if (!value) {
          return nullptr;
        }

        RootedValue value_val(cx, JS::StringValue(value));
        auto res = FormData::append(cx, formdata, name, value_val, UndefinedHandleValue);
        if (!res) {
          return nullptr;
        }
      } else {
        // Each part whose `Content-Disposition` header contains a `filename` parameter
        // must be parsed into an entry whose value is a File object whose contents are
        // the contents of the part. The name attribute of the File object must have the
        // value of the `filename` parameter of the part. The type attribute of the File
        // object must have the value of the `Content-Type` header of the part if the part
        // has such header, and `text/plain` otherwise.
        RootedObject filebits(cx, to_owned_buffer(cx, entry.value));
        if (!filebits) {
          return nullptr;
        }

        RootedString filename(cx, to_owned_string(cx, entry.filename));
        if (!filename) {
          return nullptr;
        }

        RootedValue content_type_val(cx);
        if (entry.content_type.data && (entry.content_type.len != 0U)) {
          RootedString content_type(cx, to_owned_string(cx, entry.content_type));
          if (!content_type) {
            return nullptr;
          }

          content_type_val = JS::StringValue(content_type);
        } else {
          RootedString content_type(cx, JS_NewStringCopyN(cx, "text/plain", 10));
          if (!content_type) {
            return nullptr;
          }

          content_type_val = JS::StringValue(content_type);
        }

        RootedObject opts(cx, JS_NewPlainObject(cx));
        if (!opts) {
          return nullptr;
        }

        if (!JS_DefineProperty(cx, opts, "type", content_type_val, JSPROP_ENUMERATE)) {
          return nullptr;
        }

        RootedValue filebits_val(cx, JS::ObjectValue(*filebits));
        RootedValue filename_val(cx, JS::StringValue(filename));
        RootedValue opts_val(cx, JS::ObjectValue(*opts));

        RootedObject file(cx, File::create(cx, filebits_val, filename_val, opts_val));
        if (!file) {
          return nullptr;
        }

        RootedValue value_val(cx, JS::ObjectValue(*file));
        auto res = FormData::append(cx, formdata, name, value_val, UndefinedHandleValue);
        if (!res) {
          return nullptr;
        }
      }

      break;
    }
    }
  }

  // Return a new FormData object, appending each entry, resulting from the parsing
  // operation, to its entry list.
  return formdata;
}

class UrlParser : public FormDataParser {
  JSObject *parse(JSContext *cx, std::string_view body) override;
};

JSObject *UrlParser::parse(JSContext *cx, std::string_view body) {
  RootedObject formdata(cx, FormData::create(cx));
  if (!formdata) {
    return nullptr;
  }

  if (body.empty()) {
    return formdata;
  }

  jsurl::SpecString spec((uint8_t *)body.data(), body.size(), body.size());

  auto deleter = [&](auto *params) { jsurl::free_params(params); };
  std::unique_ptr<jsurl::JSUrlSearchParams, decltype(deleter)> params(jsurl::new_params(), deleter);
  if (!params) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  jsurl::params_init(params.get(), &spec);
  auto index = 0;

  jsurl::JSSearchParam param{};

  while (true) {
    jsurl::params_at(params.get(), index, &param);
    if (param.done || param.name.data == nullptr || param.value.data == nullptr) {
      break;
    }

    auto val_chars = JS::UTF8Chars((char *)param.value.data, param.value.len);
    JS::RootedString val_str(cx, JS_NewStringCopyUTF8N(cx, val_chars));
    if (!val_str) {
      JS_ReportOutOfMemory(cx);
      return nullptr;
    }

    auto name = std::string_view((char *)param.name.data, param.name.len);
    JS::RootedValue value_val(cx, JS::StringValue(val_str));

    auto res = FormData::append(cx, formdata, name, value_val, UndefinedHandleValue);
    if (!res) {
      return nullptr;
    }

    index += 1;
  }

  return formdata;
}

std::unique_ptr<FormDataParser> FormDataParser::create(std::string_view content_type) {
  if (content_type.starts_with("multipart/form-data")) {
    jsmultipart::Slice content_slice{.data=(uint8_t *)(content_type.data()), .len=content_type.size()};
    jsmultipart::Slice boundary_slice{.data=nullptr, .len=0};

    jsmultipart::boundary_from_content_type(&content_slice, &boundary_slice);
    if (boundary_slice.data == nullptr) {
      return nullptr;
    }

    std::string_view boundary((char *)boundary_slice.data, boundary_slice.len);
    return std::make_unique<MultipartParser>(boundary);
  }

  if (content_type.starts_with("application/x-www-form-urlencoded")) {
    return std::make_unique<UrlParser>();
  }

  if (content_type.starts_with("text/plain")) {
    // TODO: add plain text content parser
  }

  return nullptr;
}

} // namespace builtins::web::form_data


