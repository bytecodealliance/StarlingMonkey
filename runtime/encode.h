#ifndef JS_COMPUTE_RUNTIME_ENCODE_H
#define JS_COMPUTE_RUNTIME_ENCODE_H

#include "host_api.h"
#include "builtin.h"

namespace core {

DEF_ERR(ByteStringEncodingError, JSEXN_TYPEERR, "Cannot convert JS string into byte string", 0)

// TODO(performance): introduce a version that writes into an existing buffer,
// and use that with the hostcall buffer where possible.
// https://github.com/fastly/js-compute-runtime/issues/215
host_api::HostString encode(JSContext *cx, JS::HandleString str);
host_api::HostString encode(JSContext *cx, JS::HandleValue val);
host_api::HostString encode_byte_string(JSContext *cx, JS::HandleValue val);

jsurl::SpecString encode_spec_string(JSContext *cx, JS::HandleValue val);

// Convert the given `value` to a string and ensure that it's a scalar value string
// (https://infra.spec.whatwg.org/#scalar-value-string)
template <typename T>
JSString* to_scalar_value_string(JSContext* cx, T value) {
  auto str = encode(cx, value);
  return str ? decode(cx, str) : nullptr;
}

} // namespace core

#endif
