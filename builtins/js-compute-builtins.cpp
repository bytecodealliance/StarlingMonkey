#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <vector>

#include "js-compute-builtins.h"
#include "rust-url/rust-url.h"

#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "mozilla/Result.h"

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#include "js/experimental/TypedData.h"
#pragma clang diagnostic pop

#include "js/JSON.h"
#include "js/Stream.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "js/experimental/TypedData.h"
#include "js/friend/DumpFunctions.h"
#include "js/shadow/Object.h"
#include "zlib.h"

#include "host_interface/host_api.h"

#include "builtin.h"
#include "builtins/backend.h"
#include "builtins/cache-override.h"
#include "builtins/cache-simple.h"
#include "builtins/client-info.h"
#include "builtins/compression-stream.h"
#include "builtins/config-store.h"
#include "builtins/crypto.h"
#include "builtins/decompression-stream.h"
#include "builtins/dictionary.h"
#include "builtins/env.h"
#include "builtins/fastly.h"
#include "builtins/fetch-event.h"
#include "builtins/headers.h"
#include "builtins/kv-store.h"
#include "builtins/logger.h"
#include "builtins/native-stream-sink.h"
#include "builtins/native-stream-source.h"
#include "builtins/request-response.h"
#include "builtins/secret-store.h"
#include "builtins/shared/console.h"
#include "builtins/shared/dom-exception.h"
#include "builtins/shared/performance.h"
#include "builtins/shared/text-decoder.h"
#include "builtins/shared/text-encoder.h"
#include "builtins/shared/url.h"
#include "builtins/subtle-crypto.h"
#include "builtins/transform-stream-default-controller.h"
#include "builtins/transform-stream.h"
#include "builtins/worker-location.h"
#include "core/encode.h"
#include "core/event_loop.h"

using namespace std::literals;

using std::chrono::ceil;
using std::chrono::milliseconds;
using std::chrono::system_clock;

using builtins::Console;

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::UniqueChars;

using JS::ObjectOrNullValue;
using JS::ObjectValue;
using JS::PrivateValue;
using JS::Value;

using JS::RootedObject;
using JS::RootedString;
using JS::RootedValue;

using JS::HandleObject;
using JS::HandleString;
using JS::HandleValue;
using JS::HandleValueArray;
using JS::MutableHandleValue;

using JS::PersistentRooted;

const JSErrorFormatString *GetErrorMessage(void *userRef, unsigned errorNumber) {
  if (errorNumber > 0 && errorNumber < JSErrNum_Limit) {
    return &js_ErrorFormatString[errorNumber];
  }

  return nullptr;
}

enum class Mode { PreWizening, PostWizening };

Mode execution_mode = Mode::PreWizening;

bool hasWizeningFinished() { return execution_mode == Mode::PostWizening; }

bool isWizening() { return execution_mode == Mode::PreWizening; }

void markWizeningAsFinished() { execution_mode = Mode::PostWizening; }

template <InternalMethod fun>
bool enqueue_internal_method(JSContext *cx, HandleObject receiver,
                             HandleValue extra = JS::UndefinedHandleValue, unsigned int nargs = 0,
                             const char *name = "") {
  RootedObject method(cx, create_internal_method<fun>(cx, receiver, extra, nargs, name));
  if (!method) {
    return false;
  }

  RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  if (!promise) {
    return false;
  }

  return JS::AddPromiseReactions(cx, promise, method, nullptr);
}

using jsurl::SpecSlice, jsurl::SpecString, jsurl::JSUrl, jsurl::JSUrlSearchParams,
    jsurl::JSSearchParam;

bool RejectPromiseWithPendingError(JSContext *cx, HandleObject promise) {
  RootedValue exn(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &exn)) {
    return false;
  }
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx, promise, exn);
}

JSObject *PromiseRejectedWithPendingError(JSContext *cx) {
  RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !RejectPromiseWithPendingError(cx, promise)) {
    return nullptr;
  }
  return promise;
}

constexpr size_t HANDLE_READ_CHUNK_SIZE = 8192;

struct ReadResult {
  UniqueChars buffer;
  size_t length;
};

// Returns a UniqueChars and the length of that string. The UniqueChars value is not
// null-terminated.
ReadResult read_from_handle_all(JSContext *cx, uint32_t handle) {
  std::vector<host_api::HostString> chunks;
  size_t bytes_read = 0;
  host_api::HttpBody body{handle};
  while (true) {
    auto res = body.read(HANDLE_READ_CHUNK_SIZE);
    if (auto *err = res.to_err()) {
      HANDLE_ERROR(cx, *err);
      return {nullptr, 0};
    }

    auto &chunk = res.unwrap();
    if (chunk.len == 0) {
      break;
    }

    bytes_read += chunk.len;
    chunks.emplace_back(std::move(chunk));
  }

  UniqueChars buf;
  if (chunks.size() == 1) {
    // If there was only one chunk read, reuse that allocation.
    auto &chunk = chunks.back();
    buf = std::move(chunk.ptr);
  } else {
    // If there wasn't exactly one chunk read, we'll need to allocate a buffer to store the results.
    buf.reset(static_cast<char *>(JS_string_malloc(cx, bytes_read)));
    if (!buf) {
      JS_ReportOutOfMemory(cx);
      return {nullptr, 0};
    }

    char *end = buf.get();
    for (auto &chunk : chunks) {
      end = std::copy(chunk.ptr.get(), chunk.ptr.get() + chunk.len, end);
    }
  }

  return {std::move(buf), bytes_read};
}

namespace GlobalProperties {

JS::Result<std::string> convertJSValueToByteString(JSContext *cx, JS::Handle<JS::Value> v) {
  JS::RootedString s(cx);
  if (v.isString()) {
    s = v.toString();
  } else {
    s = JS::ToString(cx, v);
    if (!s) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_CHARACTER_ERROR);
      return JS::Result<std::string>(JS::Error());
    }
  }

  // Conversion from JavaScript string to ByteString is only valid if all
  // characters < 256. This is always the case for Latin1 strings.
  size_t length;
  if (!JS::StringHasLatin1Chars(s)) {
    // Creating an exception can GC, so we first scan the string for bad chars
    // and report the error outside the AutoCheckCannotGC scope.
    bool foundBadChar = false;
    {
      JS::AutoCheckCannotGC nogc;
      const char16_t *chars = JS_GetTwoByteStringCharsAndLength(cx, nogc, s, &length);
      if (!chars) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_CHARACTER_ERROR);
        return JS::Result<std::string>(JS::Error());
      }

      for (size_t i = 0; i < length; i++) {
        if (chars[i] > 255) {
          foundBadChar = true;
          break;
        }
      }
    }

    if (foundBadChar) {
      JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_CHARACTER_ERROR);
      return JS::Result<std::string>(JS::Error());
    }
  } else {
    length = JS::GetStringLength(s);
  }

  UniqueChars result = JS_EncodeStringToLatin1(cx, s);
  if (!result) {
    return JS::Result<std::string>(JS::Error());
  }
  std::string byteString(result.get(), length);
  return byteString;
}

JS::Result<std::string> convertJSValueToByteString(JSContext *cx, std::string v) {
  JS::RootedValue s(cx);
  s.setString(JS_NewStringCopyN(cx, v.c_str(), v.length()));
  return convertJSValueToByteString(cx, s);
}

// Maps an encoded character to a value in the Base64 alphabet, per
// RFC 4648, Table 1. Invalid input characters map to UINT8_MAX.
// https://datatracker.ietf.org/doc/html/rfc4648#section-4

constexpr uint8_t nonAlphabet = 255;

// clang-format off
const uint8_t base64DecodeTable[128] = {
  /* 0 */    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 8 */    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 16 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 24 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 32 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 40 */   nonAlphabet, nonAlphabet, nonAlphabet,          62, nonAlphabet, nonAlphabet, nonAlphabet,          63,
  /* 48 */            52,          53,          54,          55,          56,          57,          58,          59,
  /* 56 */            60,          61, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 64 */   nonAlphabet,           0,           1,           2,           3,           4,           5,           6,
  /* 72 */             7,           8,           9,          10,          11,          12,          13,          14,
  /* 80 */            15,          16,          17,          18,          19,          20,          21,          22,
  /* 88 */            23,          24,          25, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,          63,
  /* 96 */   nonAlphabet,          26,          27,          28,          29,          30,          31,          32,
  /* 104 */           33,          34,          35,          36,          37,          38,          39,          40,
  /* 112 */           41,          42,          43,          44,          45,          46,          47,          48,
  /* 120 */           49,          50,          51, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet
};

const uint8_t base64URLDecodeTable[128] = { 
  /* 0 */    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 8 */    nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 16 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 24 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 32 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 40 */   nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,          62, nonAlphabet, nonAlphabet,
  /* 48 */            52,          53,          54,          55,          56,          57,          58,          59,
  /* 56 */            60,          61, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,
  /* 64 */   nonAlphabet,           0,           1,           2,           3,           4,           5,           6,
  /* 72 */             7,           8,           9,          10,          11,          12,          13,          14,
  /* 80 */            15,          16,          17,          18,          19,          20,          21,          22,
  /* 88 */            23,          24,          25, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet,          63,
  /* 96 */   nonAlphabet,          26,          27,          28,          29,          30,          31,          32,
  /* 104 */           33,          34,          35,          36,          37,          38,          39,          40,
  /* 112 */           41,          42,          43,          44,          45,          46,          47,          48,
  /* 120 */           49,          50,          51, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet, nonAlphabet
};

const char base64EncodeTable[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                      "abcdefghijklmnopqrstuvwxyz"
                      "0123456789+/";

const char base64URLEncodeTable[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                      "abcdefghijklmnopqrstuvwxyz"
                      "0123456789-_";

// clang-format on

bool base64CharacterToValue(char character, uint8_t *value, const uint8_t *decodeTable) {
  static const size_t mask = 127;
  auto index = static_cast<size_t>(character);

  if (index & ~mask) {
    return false;
  }
  *value = decodeTable[index & mask];

  return *value != 255;
}

inline JS::Result<mozilla::Ok> base64Decode4to3(std::string_view input, std::string &output,
                                                const uint8_t *decodeTable) {
  uint8_t w, x, y, z;
  // 8.1 Find the code point pointed to by position in the second column of Table 1: The Base 64
  // Alphabet of RFC 4648. Let n be the number given in the first cell of the same row. [RFC4648]
  if (!base64CharacterToValue(input[0], &w, decodeTable) ||
      !base64CharacterToValue(input[1], &x, decodeTable) ||
      !base64CharacterToValue(input[2], &y, decodeTable) ||
      !base64CharacterToValue(input[3], &z, decodeTable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }

  // 8.3 If buffer has accumulated 24 bits, interpret them as three 8-bit big-endian numbers. Append
  // three bytes with values equal to those numbers to output, in the same order, and then empty
  // buffer.
  output += (uint8_t(w << 2 | x >> 4));
  output += (uint8_t(x << 4 | y >> 2));
  output += (uint8_t(y << 6 | z));
  return mozilla::Ok();
}

inline JS::Result<mozilla::Ok> base64Decode3to2(std::string_view input, std::string &output,
                                                const uint8_t *decodeTable) {
  uint8_t w, x, y;
  // 8.1 Find the code point pointed to by position in the second column of Table 1: The Base 64
  // Alphabet of RFC 4648. Let n be the number given in the first cell of the same row. [RFC4648]
  if (!base64CharacterToValue(input[0], &w, decodeTable) ||
      !base64CharacterToValue(input[1], &x, decodeTable) ||
      !base64CharacterToValue(input[2], &y, decodeTable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }
  // 9. If buffer is not empty, it contains either 12 or 18 bits. If it contains 12 bits, then
  // discard the last four and interpret the remaining eight as an 8-bit big-endian number. If it
  // contains 18 bits, then discard the last two and interpret the remaining 16 as two 8-bit
  // big-endian numbers. Append the one or two bytes with values equal to those one or two numbers
  // to output, in the same order.
  output += (uint8_t(w << 2 | x >> 4));
  output += (uint8_t(x << 4 | y >> 2));
  return mozilla::Ok();
}

inline JS::Result<mozilla::Ok> base64Decode2to1(std::string_view input, std::string &output,
                                                const uint8_t *decodeTable) {
  uint8_t w, x;
  // 8.1 Find the code point pointed to by position in the second column of Table 1: The Base 64
  // Alphabet of RFC 4648. Let n be the number given in the first cell of the same row. [RFC4648]
  if (!base64CharacterToValue(input[0], &w, decodeTable) ||
      !base64CharacterToValue(input[1], &x, decodeTable)) {
    return JS::Result<mozilla::Ok>(JS::Error());
  }
  // 9. If buffer is not empty, it contains either 12 or 18 bits. If it contains 12 bits, then
  // discard the last four and interpret the remaining eight as an 8-bit big-endian number. If it
  // contains 18 bits, then discard the last two and interpret the remaining 16 as two 8-bit
  // big-endian numbers. Append the one or two bytes with values equal to those one or two numbers
  // to output, in the same order.
  output += (uint8_t(w << 2 | x >> 4));
  return mozilla::Ok();
}

bool isAsciiWhitespace(char c) {
  switch (c) {
  case '\t':
  case '\n':
  case '\f':
  case '\r':
  case ' ':
    return true;
  default:
    return false;
  }
}

// https://infra.spec.whatwg.org/#forgiving-base64-decode
JS::Result<std::string> forgivingBase64Decode(std::string_view data,
                                              const uint8_t *decodeTable = base64DecodeTable) {
  // 1. Remove all ASCII whitespace from data.
  // ASCII whitespace is U+0009 TAB, U+000A LF, U+000C FF, U+000D CR, or U+0020 SPACE.
  auto hasWhitespace = std::find_if(data.begin(), data.end(), &isAsciiWhitespace);
  std::string dataWithoutAsciiWhitespace;

  if (hasWhitespace) {
    dataWithoutAsciiWhitespace = data;
    dataWithoutAsciiWhitespace.erase(std::remove_if(dataWithoutAsciiWhitespace.begin() +
                                                        std::distance(data.begin(), hasWhitespace),
                                                    dataWithoutAsciiWhitespace.end(),
                                                    &isAsciiWhitespace),
                                     dataWithoutAsciiWhitespace.end());
    data = dataWithoutAsciiWhitespace;
  }
  std::string_view data_view(data);
  size_t length = data_view.length();

  // 2. If data’s code point length divides by 4 leaving no remainder, then:
  if (length && (length % 4 == 0)) {
    // 2.1 If data ends with one or two U+003D (=) code points, then remove them from data.
    if (data_view.at(length - 1) == '=') {
      if (data_view.at(length - 2) == '=') {
        data_view.remove_suffix(2);
      } else {
        data_view.remove_suffix(1);
      }
    }
  }

  // 3. If data’s code point length divides by 4 leaving a remainder of 1, then return failure.
  if ((data_view.length() % 4 == 1)) {
    return JS::Result<std::string>(JS::Error());
  }

  // 4. If data contains a code point that is not one of
  //    U+002B (+)
  //    U+002F (/)
  //    ASCII alphanumeric
  // then return failure.

  // Step 4 is handled within the calls below to
  // base64Decode4to3, base64Decode3to2, and base64Decode2to1

  // 5. Let output be an empty byte sequence.
  std::string output = "";
  output.reserve(data_view.length() / 3);

  // 6. Let buffer be an empty buffer that can have bits appended to it.

  // Step 6 is handled within the calls below to
  // base64Decode4to3, base64Decode3to2, and base64Decode2to1

  // 7. Let position be a position variable for data, initially pointing at the start of data.

  // We don't use a position variable, instead we remove_prefix from the `data` each time we have
  // dealt with some characters.

  while (data_view.length() >= 4) {
    MOZ_TRY(base64Decode4to3(data_view, output, decodeTable));
    data_view.remove_prefix(4);
  }

  switch (data_view.length()) {
  case 3: {
    MOZ_TRY(base64Decode3to2(data_view, output, decodeTable));
    break;
  }
  case 2: {
    MOZ_TRY(base64Decode2to1(data_view, output, decodeTable));
    break;
  }
  case 1:
    return JS::Result<std::string>(JS::Error());
  case 0:
    break;
  default:
    MOZ_CRASH("Too many characters leftover");
  }

  return output;
}

// https://html.spec.whatwg.org/multipage/webappapis.html#dom-atob
bool atob(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "atob", 1)) {
    return false;
  }
  auto dataResult = convertJSValueToByteString(cx, args.get(0));
  if (dataResult.isErr()) {
    return false;
  }
  auto data = dataResult.unwrap();

  // 1. Let decodedData be the result of running forgiving-base64 decode on data.
  auto decoded_result = forgivingBase64Decode(data);
  // 2. If decodedData is failure, then throw an "InvalidCharacterError" DOMException.
  if (decoded_result.isErr()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_CHARACTER_ERROR);
    return false;
  }
  auto decoded = decoded_result.unwrap();
  RootedString decodedData(cx, JS_NewStringCopyN(cx, decoded.c_str(), decoded.length()));
  if (!decodedData) {
    return false;
  }

  // 3. Return decodedData.
  args.rval().setString(decodedData);
  return true;
}

inline uint8_t CharTo8Bit(char character) { return uint8_t(character); }
inline void base64Encode3to4(std::string_view data, std::string &output, const char *encodeTable) {
  uint32_t b32 = 0;
  int i, j = 18;

  for (i = 0; i < 3; ++i) {
    b32 <<= 8;
    b32 |= CharTo8Bit(data[i]);
  }

  for (i = 0; i < 4; ++i) {
    output += encodeTable[(uint32_t)((b32 >> j) & 0x3F)];
    j -= 6;
  }
}

inline void base64Encode2to4(std::string_view data, std::string &output, const char *encodeTable) {
  uint8_t src0 = CharTo8Bit(data[0]);
  uint8_t src1 = CharTo8Bit(data[1]);
  output += encodeTable[(uint32_t)((src0 >> 2) & 0x3F)];
  output += encodeTable[(uint32_t)(((src0 & 0x03) << 4) | ((src1 >> 4) & 0x0F))];
  output += encodeTable[(uint32_t)((src1 & 0x0F) << 2)];
  output += '=';
}

inline void base64Encode1to4(std::string_view data, std::string &output, const char *encodeTable) {
  uint8_t src0 = CharTo8Bit(data[0]);
  output += encodeTable[(uint32_t)((src0 >> 2) & 0x3F)];
  output += encodeTable[(uint32_t)((src0 & 0x03) << 4)];
  output += '=';
  output += '=';
}

// https://infra.spec.whatwg.org/#forgiving-base64-encode
// To forgiving-base64 encode given a byte sequence data, apply the base64 algorithm defined in
// section 4 of RFC 4648 to data and return the result. [RFC4648] Note: This is named
// forgiving-base64 encode for symmetry with forgiving-base64 decode, which is different from the
// RFC as it defines error handling for certain inputs.
std::string forgivingBase64Encode(std::string_view data, const char *encodeTable) {
  int length = data.length();
  std::string output = "";
  // The Base64 version of a string will be at least 133% the size of the string.
  output.reserve(length * 1.33);
  while (length >= 3) {
    base64Encode3to4(data, output, encodeTable);
    data.remove_prefix(3);
    length -= 3;
  }

  switch (length) {
  case 2:
    base64Encode2to4(data, output, encodeTable);
    break;
  case 1:
    base64Encode1to4(data, output, encodeTable);
    break;
  case 0:
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("coding error");
  }
  return output;
}

// The btoa(data) method must throw an "InvalidCharacterError" DOMException
// if data contains any character whose code point is greater than U+00FF.
// Otherwise, the user agent must convert data to a byte sequence whose
// nth byte is the eight-bit representation of the nth code point of data,
// and then must apply forgiving-base64 encode to that byte sequence and return the result.
bool btoa(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.requireAtLeast(cx, "btoa", 1)) {
    return false;
  }

  auto data = args.get(0);
  auto out = args.rval();
  // Note: We do not check if data contains any character whose code point is greater than U+00FF
  // before calling convertJSValueToByteString as convertJSValueToByteString does the same check
  auto byteStringResult = convertJSValueToByteString(cx, data);
  if (byteStringResult.isErr()) {
    return false;
  }
  auto byteString = byteStringResult.unwrap();

  auto result = forgivingBase64Encode(byteString, GlobalProperties::base64EncodeTable);

  JSString *str = JS_NewStringCopyN(cx, result.c_str(), result.length());
  if (!str) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr, JSMSG_INVALID_CHARACTER_ERROR);

    return false;
  }

  out.setString(str);
  return true;
}

// TODO: throw in all Request methods/getters that rely on host calls once a
// request has been sent. The host won't let us act on them anymore anyway.
/**
 * The `fetch` global function
 * https://fetch.spec.whatwg.org/#fetch-method
 */
bool fetch(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  REQUEST_HANDLER_ONLY("fetch")

  if (!args.requireAtLeast(cx, "fetch", 1)) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedObject requestInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::Request::class_, builtins::Request::proto_obj));
  if (!requestInstance)
    return false;

  RootedObject request(cx, builtins::Request::create(cx, requestInstance, args[0], args.get(1)));
  if (!request) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedString backend(cx, builtins::Request::backend(request));
  if (!backend) {
    if (builtins::Fastly::allowDynamicBackends) {
      JS::RootedObject dynamicBackend(cx, builtins::Backend::create(cx, request));
      if (!dynamicBackend) {
        return false;
      }
      backend.set(builtins::Backend::name(cx, dynamicBackend));
    } else {
      backend = builtins::Fastly::defaultBackend;
      if (!backend) {
        auto handle = builtins::Request::request_handle(request);

        auto res = handle.get_uri();
        if (auto *err = res.to_err()) {
          HANDLE_ERROR(cx, *err);
        } else {
          JS_ReportErrorLatin1(cx,
                               "No backend specified for request with url %s. "
                               "Must provide a `backend` property on the `init` object "
                               "passed to either `new Request()` or `fetch`",
                               res.unwrap().begin());
        }
        return ReturnPromiseRejectedWithPendingError(cx, args);
      }
    }
  }

  host_api::HostString backend_chars = core::encode(cx, backend);
  if (!backend_chars.ptr) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  RootedObject response_promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!response_promise)
    return ReturnPromiseRejectedWithPendingError(cx, args);

  if (!builtins::Request::apply_cache_override(cx, request)) {
    return false;
  }

  if (!builtins::Request::apply_auto_decompress_gzip(cx, request)) {
    return false;
  }

  bool streaming = false;
  if (!builtins::RequestOrResponse::maybe_stream_body(cx, request, &streaming)) {
    return false;
  }

  host_api::HttpPendingReq pending_handle;
  {
    auto request_handle = builtins::Request::request_handle(request);
    auto body = builtins::RequestOrResponse::body_handle(request);
    auto res = streaming ? request_handle.send_async_streaming(body, backend_chars)
                         : request_handle.send_async(body, backend_chars);

    if (auto *err = res.to_err()) {
      if (host_api::error_is_generic(*err) || host_api::error_is_invalid_argument(*err)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_REQUEST_BACKEND_DOES_NOT_EXIST, backend_chars.ptr.get());
      } else {
        HANDLE_ERROR(cx, *err);
      }
      return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    pending_handle = res.unwrap();
  }

  // If the request body is streamed, we need to wait for streaming to complete before marking the
  // request as pending.
  if (!streaming) {
    if (!core::EventLoop::queue_async_task(request))
      return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  JS::SetReservedSlot(request, static_cast<uint32_t>(builtins::Request::Slots::PendingRequest),
                      JS::Int32Value(pending_handle.handle));
  JS::SetReservedSlot(request, static_cast<uint32_t>(builtins::Request::Slots::ResponsePromise),
                      JS::ObjectValue(*response_promise));

  args.rval().setObject(*response_promise);
  return true;
}

/**
 * The `queueMicrotask` global function
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#microtask-queuing
 */
bool queueMicrotask(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "queueMicrotask", 1)) {
    return false;
  }

  if (!args[0].isObject() || !JS::IsCallable(&args[0].toObject())) {
    JS_ReportErrorLatin1(cx, "queueMicrotask: Argument 1 is not a function");
    return false;
  }

  RootedObject callback(cx, &args[0].toObject());

  RootedObject promise(cx, JS::CallOriginalPromiseResolve(cx, JS::UndefinedHandleValue));
  if (!promise) {
    return false;
  }

  if (!JS::AddPromiseReactions(cx, promise, callback, nullptr)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

bool self_get(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setObject(*JS::CurrentGlobalOrNull(cx));
  return true;
}

bool self_set(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "globalThis.self setter", 1)) {
    return false;
  }

  RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  if (args.thisv() != ObjectValue(*global)) {
    JS_ReportErrorLatin1(cx, "globalThis.self setter can only be called on the global object");
    return false;
  }

  if (!JS_DefineProperty(cx, global, "self", args[0], JSPROP_ENUMERATE)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

// Magic number used in structured cloning as a tag to identify a
// URLSearchParam.
#define SCTAG_DOM_URLSEARCHPARAMS JS_SCTAG_USER_MIN

/**
 * Reads non-JS builtins during structured cloning.
 *
 * Currently the only relevant builtin is URLSearchParams, but that'll grow to
 * include Blob and FormData, too.
 *
 * TODO: Add support for CryptoKeys
 */
JSObject *ReadStructuredClone(JSContext *cx, JSStructuredCloneReader *r,
                              const JS::CloneDataPolicy &cloneDataPolicy, uint32_t tag,
                              uint32_t len, void *closure) {
  MOZ_ASSERT(tag == SCTAG_DOM_URLSEARCHPARAMS);

  RootedObject urlSearchParamsInstance(
      cx, JS_NewObjectWithGivenProto(cx, &builtins::URLSearchParams::class_,
                                     builtins::URLSearchParams::proto_obj));
  RootedObject params_obj(cx, builtins::URLSearchParams::create(cx, urlSearchParamsInstance));
  if (!params_obj) {
    return nullptr;
  }

  void *bytes = JS_malloc(cx, len);
  if (!bytes) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  if (!JS_ReadBytes(r, bytes, len)) {
    return nullptr;
  }

  SpecString init((uint8_t *)bytes, len, len);
  jsurl::params_init(builtins::URLSearchParams::get_params(params_obj), &init);

  return params_obj;
}

/**
 * Writes non-JS builtins during structured cloning.
 *
 * Currently the only relevant builtin is URLSearchParams, but that'll grow to
 * include Blob and FormData, too.
 *
 * TODO: Add support for CryptoKeys
 */
bool WriteStructuredClone(JSContext *cx, JSStructuredCloneWriter *w, JS::HandleObject obj,
                          bool *sameProcessScopeRequired, void *closure) {
  if (!builtins::URLSearchParams::is_instance(obj)) {
    JS_ReportErrorLatin1(cx, "The object could not be cloned");
    return false;
  }

  auto slice = builtins::URLSearchParams::serialize(cx, obj);
  if (!JS_WriteUint32Pair(w, SCTAG_DOM_URLSEARCHPARAMS, slice.len) ||
      !JS_WriteBytes(w, (void *)slice.data, slice.len)) {
    return false;
  }

  return true;
}

JSStructuredCloneCallbacks sc_callbacks = {ReadStructuredClone, WriteStructuredClone};

/**
 * The `structuredClone` global function
 * https://html.spec.whatwg.org/multipage/structured-data.html#dom-structuredclone
 */
bool structuredClone(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "structuredClone", 1)) {
    return false;
  }

  RootedValue transferables(cx);
  if (args.get(1).isObject()) {
    RootedObject options(cx, &args[1].toObject());
    if (!JS_GetProperty(cx, options, "transfer", &transferables)) {
      return false;
    }
  }

  JSAutoStructuredCloneBuffer buf(JS::StructuredCloneScope::SameProcess, &sc_callbacks, nullptr);
  JS::CloneDataPolicy policy;

  if (!buf.write(cx, args[0], transferables, policy)) {
    return false;
  }

  return buf.read(cx, args.rval());
}

/**
 * The `setTimeout` and `setInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval
 */
template <bool repeat> bool setTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  REQUEST_HANDLER_ONLY(repeat ? "setInterval" : "setTimeout");
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, repeat ? "setInterval" : "setTimeout", 1)) {
    return false;
  }

  if (!(args[0].isObject() && JS::IsCallable(&args[0].toObject()))) {
    JS_ReportErrorASCII(cx, "First argument to %s must be a function",
                        repeat ? "setInterval" : "setTimeout");
    return false;
  }
  RootedObject handler(cx, &args[0].toObject());

  int32_t delay = 0;
  if (args.length() > 1 && !JS::ToInt32(cx, args.get(1), &delay)) {
    return false;
  }
  if (delay < 0) {
    delay = 0;
  }

  JS::RootedValueVector handler_args(cx);
  if (args.length() > 2 && !handler_args.initCapacity(args.length() - 2)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  for (size_t i = 2; i < args.length(); i++) {
    handler_args.infallibleAppend(args[i]);
  }

  uint32_t id = core::EventLoop::add_timer(handler, delay, handler_args, repeat);

  args.rval().setInt32(id);
  return true;
}

/**
 * The `clearTimeout` and `clearInterval` global functions
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-cleartimeout
 * https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-clearinterval
 */
template <bool interval> bool clearTimeout_or_interval(JSContext *cx, unsigned argc, Value *vp) {
  // REQUEST_HANDLER_ONLY(interval ? "clearInterval" : "clearTimeout");
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, interval ? "clearInterval" : "clearTimeout", 1)) {
    return false;
  }

  int32_t id = 0;
  if (!JS::ToInt32(cx, args[0], &id)) {
    return false;
  }

  core::EventLoop::remove_timer(uint32_t(id));

  args.rval().setUndefined();
  return true;
}

const JSFunctionSpec methods[] = {
    JS_FN("atob", atob, 1, JSPROP_ENUMERATE),
    JS_FN("btoa", btoa, 1, JSPROP_ENUMERATE),
    JS_FN("clearInterval", clearTimeout_or_interval<true>, 1, JSPROP_ENUMERATE),
    JS_FN("clearTimeout", clearTimeout_or_interval<false>, 1, JSPROP_ENUMERATE),
    JS_FN("fetch", fetch, 2, JSPROP_ENUMERATE),
    JS_FN("queueMicrotask", queueMicrotask, 1, JSPROP_ENUMERATE),
    JS_FN("setInterval", setTimeout_or_interval<true>, 1, JSPROP_ENUMERATE),
    JS_FN("setTimeout", setTimeout_or_interval<false>, 1, JSPROP_ENUMERATE),
    JS_FN("structuredClone", structuredClone, 1, JSPROP_ENUMERATE),
    JS_FN("structuredClone", structuredClone, 1, JSPROP_ENUMERATE),
    JS_FS_END};

const JSPropertySpec properties[] = {JS_PSGS("self", self_get, self_set, JSPROP_ENUMERATE),
                                     JS_PS_END};

static bool init(JSContext *cx, HandleObject global) {
  return JS_DefineFunctions(cx, global, methods) && JS_DefineProperties(cx, global, properties);
}
} // namespace GlobalProperties

bool math_random(JSContext *cx, unsigned argc, Value *vp) {
  auto res = host_api::Random::get_u32();
  MOZ_ASSERT(!res.is_err());
  double newvalue = static_cast<double>(res.unwrap()) / std::pow(2.0, 32.0);

  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setDouble(newvalue);
  return true;
}

bool define_fastly_sys(JSContext *cx, HandleObject global, FastlyOptions options) {
  if (!GlobalProperties::init(cx, global))
    return false;

  if (!builtins::DOMException::init_class(cx, global)) {
    return false;
  }
  if (!builtins::Backend::init_class(cx, global))
    return false;
  if (!builtins::Fastly::create(cx, global, options))
    return false;
  if (!builtins::Console::create(cx, global))
    return false;
  if (!builtins::SubtleCrypto::init_class(cx, global))
    return false;
  if (!builtins::Crypto::init_class(cx, global))
    return false;
  if (!builtins::CryptoKey::init_class(cx, global))
    return false;

  if (!builtins::NativeStreamSource::init_class(cx, global))
    return false;
  if (!builtins::NativeStreamSink::init_class(cx, global))
    return false;
  if (!builtins::TransformStreamDefaultController::init_class(cx, global))
    return false;
  if (!builtins::TransformStream::init_class(cx, global))
    return false;
  if (!builtins::CompressionStream::init_class(cx, global))
    return false;
  if (!builtins::DecompressionStream::init_class(cx, global))
    return false;
  if (!builtins::Request::init_class(cx, global))
    return false;
  if (!builtins::Response::init_class(cx, global))
    return false;
  if (!builtins::ConfigStore::init_class(cx, global))
    return false;
  if (!builtins::Dictionary::init_class(cx, global))
    return false;
  if (!builtins::Headers::init_class(cx, global))
    return false;
  if (!builtins::ClientInfo::init_class(cx, global))
    return false;
  if (!builtins::FetchEvent::init_class(cx, global))
    return false;
  if (!builtins::CacheOverride::init_class(cx, global))
    return false;
  if (!builtins::TextEncoder::init_class(cx, global))
    return false;
  if (!builtins::TextDecoder::init_class(cx, global))
    return false;
  if (!builtins::Logger::init_class(cx, global))
    return false;
  if (!builtins::URL::init_class(cx, global))
    return false;
  if (!builtins::URLSearchParams::init_class(cx, global))
    return false;
  if (!builtins::URLSearchParamsIterator::init_class(cx, global))
    return false;
  if (!builtins::WorkerLocation::init_class(cx, global))
    return false;
  if (!builtins::KVStore::init_class(cx, global))
    return false;
  if (!builtins::KVStoreEntry::init_class(cx, global))
    return false;
  if (!builtins::SecretStore::init_class(cx, global))
    return false;
  if (!builtins::SecretStoreEntry::init_class(cx, global))
    return false;
  if (!builtins::SimpleCache::init_class(cx, global)) {
    return false;
  }
  if (!builtins::SimpleCacheEntry::init_class(cx, global)) {
    return false;
  }
  if (!builtins::Performance::init_class(cx, global)) {
    return false;
  }
  if (!builtins::Performance::create(cx, global)) {
    return false;
  }

  core::EventLoop::init(cx);

  JS::RootedValue math_val(cx);
  if (!JS_GetProperty(cx, global, "Math", &math_val)) {
    return false;
  }
  JS::RootedObject math(cx, &math_val.toObject());

  const JSFunctionSpec funs[] = {JS_FN("random", math_random, 0, 0), JS_FS_END};
  if (!JS_DefineFunctions(cx, math, funs)) {
    return false;
  }

  return true;
}

JSObject *create_fetch_event(JSContext *cx) { return builtins::FetchEvent::create(cx); }

UniqueChars stringify_value(JSContext *cx, JS::HandleValue value) {
  JS::RootedString str(cx, JS_ValueToSource(cx, value));
  if (!str)
    return nullptr;

  return JS_EncodeStringToUTF8(cx, str);
}

bool debug_logging_enabled() { return builtins::Fastly::debug_logging_enabled; }

void builtin_impl_console_log(Console::LogType log_ty, const char *msg) {
  const char *prefix = "";
  switch (log_ty) {
  case Console::LogType::Log:
    prefix = "Log";
    break;
  case Console::LogType::Debug:
    prefix = "Debug";
    break;
  case Console::LogType::Info:
    prefix = "Info";
    break;
  case Console::LogType::Warn:
    prefix = "Warn";
    break;
  case Console::LogType::Error:
    prefix = "Error";
    break;
  }
  fprintf(stdout, "%s: %s\n", prefix, msg);
  fflush(stdout);
}

bool dump_value(JSContext *cx, JS::Value val, FILE *fp) {
  RootedValue value(cx, val);
  UniqueChars utf8chars = stringify_value(cx, value);
  if (!utf8chars) {
    return false;
  }

  fprintf(fp, "%s\n", utf8chars.get());
  return true;
}

bool print_stack(JSContext *cx, HandleObject stack, FILE *fp) {
  RootedString stackStr(cx);
  if (!BuildStackString(cx, nullptr, stack, &stackStr, 2)) {
    return false;
  }

  auto utf8chars = core::encode(cx, stackStr);
  if (!utf8chars) {
    return false;
  }

  fprintf(fp, "%s\n", utf8chars.begin());
  return true;
}

void dump_promise_rejection(JSContext *cx, HandleValue reason, HandleObject promise, FILE *fp) {
  bool reported = false;
  RootedObject stack(cx);

  if (reason.isObject()) {
    RootedObject err(cx, &reason.toObject());
    JSErrorReport *report = JS_ErrorFromException(cx, err);
    if (report) {
      fprintf(stderr, "%s\n", report->message().c_str());
      reported = true;
    }

    stack = JS::ExceptionStackOrNull(err);
  }

  // If the rejection reason isn't an `Error` object, we just dump the value
  // as-is.
  if (!reported) {
    dump_value(cx, reason, stderr);
  }

  // If the rejection reason isn't an `Error` object, we can't get an exception
  // stack from it. In that case, fall back to getting the stack from the
  // promise resolution site. These should be identical in many cases, such as
  // for exceptions thrown in async functions, but for some reason the
  // resolution site stack seems to sometimes be wrong, so we only fall back to
  // it as a last resort.
  if (!stack) {
    stack = JS::GetPromiseResolutionSite(promise);
  }

  if (stack) {
    fprintf(stderr, "Stack:\n");
    print_stack(cx, stack, stderr);
  }
}

bool print_stack(JSContext *cx, FILE *fp) {
  RootedObject stackp(cx);
  if (!JS::CaptureCurrentStack(cx, &stackp))
    return false;
  return print_stack(cx, stackp, fp);
}
