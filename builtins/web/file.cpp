#include "file.h"
#include "blob.h"
#include "js/CallArgs.h"
#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"

namespace builtins {
namespace web {
namespace file {

bool maybe_file_instance(JSContext *cx, HandleValue value, MutableHandleValue instance) {
  instance.setNull();

  JS::ForOfIterator it(cx);
  if (!it.init(value, JS::ForOfIterator::AllowNonIterable)) {
    return false;
  }

  bool is_iterable = value.isObject() && it.valueIsIterable();
  bool done;

  if (is_iterable) {
    JS::RootedValue item(cx);

    if (!it.next(&item, &done)) {
      return false;
    }
    if (item.isObject() && File::is_instance(&item.toObject())) {
      instance.setObject(item.toObject());
    }
  }

  return true;
}

using blob::Blob;

const JSFunctionSpec File::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec File::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec File::methods[] = {
    JS_FN("arrayBuffer", File::arrayBuffer, 0, JSPROP_ENUMERATE),
    JS_FN("bytes", File::bytes, 0, JSPROP_ENUMERATE),
    JS_FN("slice", File::slice, 0, JSPROP_ENUMERATE),
    JS_FN("stream", File::stream, 0, JSPROP_ENUMERATE),
    JS_FN("text", File::text, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec File::properties[] = {
    JS_PSG("size", File::size_get, JSPROP_ENUMERATE),
    JS_PSG("type", File::type_get, JSPROP_ENUMERATE),
    JS_PSG("name", File::name_get, JSPROP_ENUMERATE),
    JS_PSG("lastModified", File::lastModified_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "File", JSPROP_READONLY),
    JS_PS_END,
};

#define DEFINE_BLOB_DELEGATE(name)                             \
bool File::name(JSContext *cx, unsigned argc, JS::Value *vp) { \
  METHOD_HEADER(0)                                             \
  RootedObject blob(cx, File::blob(self));                     \
  return Blob::name(cx, blob, args.rval());                    \
}

DEFINE_BLOB_DELEGATE(arrayBuffer)
DEFINE_BLOB_DELEGATE(bytes)
DEFINE_BLOB_DELEGATE(stream)
DEFINE_BLOB_DELEGATE(text)

bool File::slice(JSContext *cx, unsigned argc, JS::Value *vp) {
    METHOD_HEADER(0)
    RootedObject blob(cx, File::blob(self));
    return Blob::slice(cx, blob, args, args.rval());
}

bool File::size_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "size get", "File");
  }

  RootedObject blob(cx, File::blob(self));
  args.rval().setNumber(Blob::blob_size(blob));
  return true;
}

bool File::type_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "type get", "File");
  }

  RootedObject blob(cx, File::blob(self));
  args.rval().setString(Blob::type(blob));
  return true;
}

bool File::name_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "name get", "File");
  }

  auto name = JS::GetReservedSlot(self, static_cast<size_t>(File::Slots::Name)).toString();
  args.rval().setString(name);
  return true;
}

bool File::lastModified_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "lastModified get", "File");
  }

  auto lastModified = JS::GetReservedSlot(self, static_cast<size_t>(File::Slots::LastModified)).toInt32();
  args.rval().setNumber(lastModified);
  return true;
}

bool File::init_last_modified(JSContext *cx, HandleObject self, HandleValue initv) {
  bool has_last_modified = false;

  JS::RootedValue init_val(cx, initv);
  JS::RootedObject opts(cx, init_val.toObjectOrNull());

  if (!opts) {
    return true;
  }

  if (!JS_HasProperty(cx, opts, "lastModified", &has_last_modified)) {
    return false;
  }

  if (has_last_modified) {
    JS::RootedValue ts(cx);
    if (!JS_GetProperty(cx, opts, "lastModified", &ts)) {
      return false;
    }

    if (ts.isNumber()) {
      SetReservedSlot(self, static_cast<uint32_t>(Slots::LastModified), ts);
    }
  }

  return true;
}

JSObject *File::blob(JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto blob = &JS::GetReservedSlot(self, static_cast<size_t>(Blob::Slots::Data)).toObject();

  MOZ_ASSERT(blob);
  return blob;
}

bool File::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("File", 2);

  RootedValue fileBits(cx, args.get(0));
  RootedValue fileName(cx, args.get(1));
  RootedValue opts(cx, args.get(2));

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }

  RootedValue other(cx);
  if (!maybe_file_instance(cx, fileBits, &other)) {
    return false;
  }

  if (!other.isNull()) {
    MOZ_ASSERT(other.isObject());
    RootedObject other_blob(cx, File::blob(&other.toObject()));
    RootedValue blob_copy(cx);

    if (!Blob::slice(cx, other_blob, CallArgs(), &blob_copy)) {
      return false;
    }

    SetReservedSlot(self, static_cast<uint32_t>(Slots::Blob), blob_copy);
  } else {
    RootedObject blob(cx, Blob::create(cx, fileBits, opts));
    if (!blob) {
      return false;
    }
    SetReservedSlot(self, static_cast<uint32_t>(Slots::Blob), JS::ObjectValue(*blob));
  }


  RootedString name(cx, JS::ToString(cx, fileName));
  if (!name) {
    return false;
  }
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Name), JS::StringValue(name));

  if (!opts.isNullOrUndefined()) {
    if (!init_last_modified(cx, self, opts)) {
      return false;
    }
  } else {
    auto now = std::chrono::system_clock::now();
    auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    SetReservedSlot(self, static_cast<uint32_t>(Slots::LastModified), JS::Int32Value(ms_since_epoch));
  }

  args.rval().setObject(*self);
  return true;
}

bool File::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

bool install(api::Engine *engine) {
    return File::init_class(engine->cx(), engine->global());
}

} // namespace file
} // namespace web
} // namespace builtins
