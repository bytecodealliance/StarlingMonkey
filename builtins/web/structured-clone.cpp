#include "structured-clone.h"
#include "blob.h"
#include "decode.h"
#include "encode.h"

#include "dom-exception.h"
#include "mozilla/Assertions.h"



namespace builtins::web::structured_clone {

// Magic number used in structured cloning as a tag to identify a
// URLSearchParam.
#define SCTAG_DOM_URLSEARCHPARAMS (JS_SCTAG_USER_MIN)
#define SCTAG_DOM_BLOB            (JS_SCTAG_USER_MIN + 1)

/**
 * Reads non-JS builtins during structured cloning.
 *
 * Currently the only relevant builtin is URLSearchParams and Blob, but that'll grow to
 * include FormData, too.
 *
 * TODO: Add support for CryptoKeys
 */
JSObject *ReadStructuredClone(JSContext *cx, JSStructuredCloneReader *r,
                              const JS::CloneDataPolicy &cloneDataPolicy, uint32_t tag,
                              uint32_t len, void *closure) {
  void *bytes = JS_malloc(cx, len);
  if (!bytes) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  if (!JS_ReadBytes(r, bytes, len)) {
    return nullptr;
  }

  switch (tag) {
  case SCTAG_DOM_URLSEARCHPARAMS: {
    RootedObject urlSearchParamsInstance(cx,
                                         JS_NewObjectWithGivenProto(cx, &url::URLSearchParams::class_,
                                                                    url::URLSearchParams::proto_obj));
    RootedObject params_obj(cx, url::URLSearchParams::create(cx, urlSearchParamsInstance));
    if (!params_obj) {
      return nullptr;
    }

    jsurl::SpecString init((uint8_t *)bytes, len, len);
    jsurl::params_init(url::URLSearchParams::get_params(params_obj), &init);

    return params_obj;

  }
  case SCTAG_DOM_BLOB: {
    UniqueChars chars(reinterpret_cast<char *>(bytes));

    // Read back the Blob's `type` written after the byte data.
    uint32_t type_len = 0;
    uint32_t unused = 0;
    if (!JS_ReadUint32Pair(r, &type_len, &unused)) {
      return nullptr;
    }

    JS::RootedString contentType(cx);
    if (type_len > 0) {
      JS::UniqueChars type_bytes(static_cast<char *>(JS_malloc(cx, type_len)));
      if (!type_bytes) {
        JS_ReportOutOfMemory(cx);
        return nullptr;
      }
      if (!JS_ReadBytes(r, type_bytes.get(), type_len)) {
        return nullptr;
      }
      contentType = core::decode(cx, std::string_view(type_bytes.get(), type_len));
    } else {
      contentType = JS_GetEmptyString(cx);
    }
    if (!contentType) {
      return nullptr;
    }

    RootedObject blob(cx, blob::Blob::create(cx, std::move(chars), len, contentType));
    return blob;
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("structured-clone undefined tag");
    return nullptr;
  }
  }
}

/**
 * Writes non-JS builtins during structured cloning.
 *
 * Currently the only relevant builtin is URLSearchParams and Blob, but that'll grow to
 * include FormData, too.
 *
 * TODO: Add support for CryptoKeys
 */
bool WriteStructuredClone(JSContext *cx, JSStructuredCloneWriter *w, JS::HandleObject obj,
                          bool *sameProcessScopeRequired, void *closure) {
  if (url::URLSearchParams::is_instance(obj)) {
    auto slice = url::URLSearchParams::serialize(cx, obj);
    if (!JS_WriteUint32Pair(w, SCTAG_DOM_URLSEARCHPARAMS, slice.len) ||
        !JS_WriteBytes(w, (void *)slice.data, slice.len)) {
      return false;
    }
  } else if (blob::Blob::is_instance(obj)) {
    auto *data = blob::Blob::blob(obj);
    // A Blob's `type` is part of its serialized state and must be preserved, so
    // it is written after the byte data as a length-prefixed UTF-8 string.
    RootedString type_str(cx, blob::Blob::type(obj));
    auto type = core::encode(cx, type_str);
    if (!type) {
      return false;
    }
    if (!JS_WriteUint32Pair(w, SCTAG_DOM_BLOB, data->length()) ||
        !JS_WriteBytes(w, (void *)data->begin(), data->length()) ||
        !JS_WriteUint32Pair(w, static_cast<uint32_t>(type.len), 0) ||
        !JS_WriteBytes(w, (void *)type.begin(), type.len)) {
      return false;
    }
  } else {
    return dom_exception::DOMException::raise(cx, "The object could not be cloned", "DataCloneError");
  }

  return true;
}

JSStructuredCloneCallbacks sc_callbacks = {.read=ReadStructuredClone, .write=WriteStructuredClone};

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

const JSFunctionSpec methods[] = {JS_FN("structuredClone", structuredClone, 1, JSPROP_ENUMERATE),
                                  JS_FS_END};

bool install(api::Engine *engine) {
  return JS_DefineFunctions(engine->cx(), engine->global(), methods);
}

} // namespace builtins::web::structured_clone


