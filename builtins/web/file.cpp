#include "file.h"
#include "blob.h"

namespace {

bool read_last_modified(JSContext *cx, HandleValue initv, int64_t *last_modified) {
  if (initv.isObject()) {
    JS::RootedObject opts(cx, &initv.toObject());
    JS::RootedValue val(cx);

    if (!JS_GetProperty(cx, opts, "lastModified", &val)) {
      return false;
    }

    if (!val.isUndefined()) {
      return ToInt64(cx, val, last_modified);
    }
  }

  // If the last modification date and time are not known, the attribute must return the
  // current date and time representing the number of milliseconds since the Unix Epoch;
  *last_modified = JS_Now() / 1000LL; // JS_Now() gives microseconds, convert it to ms.
  return true;
}

} // namespace

namespace builtins {
namespace web {
namespace file {

using blob::Blob;

const JSFunctionSpec File::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec File::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec File::methods[] = {
    JS_FS_END,
};

const JSPropertySpec File::properties[] = {
    JS_PSG("name", File::name_get, JSPROP_ENUMERATE),
    JS_PSG("lastModified", File::lastModified_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "File", JSPROP_READONLY),
    JS_PS_END,
};

bool File::name_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "name get", "File");
  }

  auto name = JS::GetReservedSlot(self, static_cast<size_t>(Slots::Name)).toString();
  args.rval().setString(name);
  return true;
}

bool File::lastModified_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "lastModified get", "File");
  }

  auto lastModified =
      JS::GetReservedSlot(self, static_cast<size_t>(Slots::LastModified)).toNumber();
  args.rval().setNumber(lastModified);
  return true;
}

// https://w3c.github.io/FileAPI/#file-constructor
bool File::init(JSContext *cx, HandleObject self, HandleValue fileBits, HandleValue fileName,
                HandleValue opts) {
  // 1. Let bytes be the result of processing blob parts given fileBits and options.
  if (!Blob::init(cx, self, fileBits, opts)) {
    return false;
  }

  // 2. Let n be the fileName argument to the constructor.
  RootedString name(cx, JS::ToString(cx, fileName));
  if (!name) {
    return false;
  }

  // 3. Process `FilePropertyBag` dictionary argument by running the following substeps:
  //  1. and 2 - the steps for processing a `type` member are ensured by Blob implementation.
  //  3. If the `lastModified` member is provided, let d be set to the lastModified dictionary
  //  member. If it is not provided, set d to the current date and time represented as the number of
  //  milliseconds since the Unix Epoch.
  int64_t lastModified;
  if (!read_last_modified(cx, opts, &lastModified)) {
    return false;
  }

  // Return a new File object F such that:
  //  2. F refers to the bytes byte sequence.
  //  3. F.size is set to the number of total bytes in bytes.
  //  4. F.name is set to n.
  //  5. F.type is set to t.
  //  6. F.lastModified is set to d.
  //
  //  Steps 2, 3 and 5 are handled by Blob. We extend the Blob by adding a `name`
  //  and the `lastModified` properties.
  SetReservedSlot(self, static_cast<uint32_t>(Slots::Name), JS::StringValue(name));
  SetReservedSlot(self, static_cast<uint32_t>(Slots::LastModified), JS::NumberValue(lastModified));

  return true;
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

  if (!init(cx, self, fileBits, fileName, opts)) {
    return false;
  }

  args.rval().setObject(*self);
  return true;
}

bool File::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global, Blob::proto_obj);
}

bool install(api::Engine *engine) { return File::init_class(engine->cx(), engine->global()); }

} // namespace file
} // namespace web
} // namespace builtins
