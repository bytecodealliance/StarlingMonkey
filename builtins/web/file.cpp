#include "file.h"
#include "blob.h"

#include "js/CallAndConstruct.h"
#include "js/CallArgs.h"
#include "js/TypeDecls.h"

#include "mozilla/Assertions.h"

namespace {

bool init_last_modified(JSContext *cx, HandleValue initv, MutableHandleValue rval) {
  JS::RootedValue init_val(cx, initv);

  if (!init_val.isNullOrUndefined()) {
    JS::RootedObject opts(cx, init_val.toObjectOrNull());

    if (opts) {
      bool has_last_modified = false;
      if (!JS_HasProperty(cx, opts, "lastModified", &has_last_modified)) {
        return false;
      }

      if (has_last_modified) {
        JS::RootedValue ts(cx);
        if (!JS_GetProperty(cx, opts, "lastModified", &ts)) {
          return false;
        }

        if (ts.isNumber()) {
          rval.set(ts);
          return true;
        }
      }
    }
  }

  // If `lastModified` is not provided, set d to the current date and time.
  auto now = std::chrono::system_clock::now();
  auto ms_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

  rval.setInt32(ms_since_epoch);
  return true;
}

} // namespace

namespace builtins {
namespace web {
namespace file {

using blob::Blob;

enum ParentSlots {
  Name = Blob::Slots::Reserved1,
  LastModified = Blob::Slots::Reserved2,
};

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

  auto name = JS::GetReservedSlot(self, static_cast<size_t>(ParentSlots::Name)).toString();
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
      JS::GetReservedSlot(self, static_cast<size_t>(ParentSlots::LastModified)).toInt32();
  args.rval().setNumber(lastModified);
  return true;
}

bool File::is_instance(const JSObject *obj) {
  return obj != nullptr && (JS::GetClass(obj) == &class_ || JS::GetClass(obj) == &Blob::class_);
}

bool File::is_instance(const Value val) { return val.isObject() && is_instance(&val.toObject()); }

bool File::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("File", 2);

  RootedValue fileBits(cx, args.get(0));
  RootedValue fileName(cx, args.get(1));
  RootedValue opts(cx, args.get(2));

  RootedObject blob_ctor(cx, JS_GetConstructor(cx, Blob::proto_obj));
  if (!blob_ctor) {
    return false;
  }

  RootedObject this_ctor(cx, JS_GetConstructor(cx, File::proto_obj));
  if (!this_ctor) {
    return false;
  }

  MOZ_ASSERT(JS::IsConstructor(blob_ctor));
  MOZ_ASSERT(JS::IsConstructor(this_ctor));

  JS::RootedValueArray<2> blob_args(cx);
  blob_args[0].set(fileBits);
  blob_args[1].set(opts);

  // 1. Let bytes be the result of processing blob parts given fileBits and options.
  //
  // We call the Blob constructor on `self` object to initialize it as a Blob.
  // We pass `fileBits` and `options` to Blob constructor.
  RootedValue blob_ctor_val(cx, JS::ObjectValue(*blob_ctor));
  RootedObject self(cx);
  if (!JS::Construct(cx, blob_ctor_val, this_ctor, blob_args, &self)) {
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
  RootedValue lastModified(cx);
  if (!init_last_modified(cx, opts, &lastModified)) {
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
  SetReservedSlot(self, static_cast<uint32_t>(ParentSlots::Name), JS::StringValue(name));
  SetReservedSlot(self, static_cast<uint32_t>(ParentSlots::LastModified), lastModified);

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
