#include "blob.h"
#include "builtin.h"
#include "encode.h"
#include "file.h"
#include "form-data.h"

#include "host_api.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

namespace {

using builtins::web::form_data::FormData;
using builtins::web::form_data::FormDataEntry;

FormDataEntry entry_from_kv_pair(std::string_view name, HandleValue value) {
    FormDataEntry entry;
    entry.name = name;
    entry.value = value;

    return entry;
}

bool name_from_args(JSContext* cx, const JS::CallArgs& args, host_api::HostString& out) {
  JS::RootedValue val(cx, args[0]);
  if (!val.isString()) {
    return false;
  }

  auto encoded = core::encode(cx, val);
  if (!encoded) {
    return false;
  }

  out = std::move(encoded);
  return true;
}

} // namespace

namespace builtins {
namespace web {
namespace form_data {

using host_api::HostString;
using blob::Blob;
using file::File;

bool FormDataIterator::next(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  JS::RootedObject form_obj(cx, &JS::GetReservedSlot(self, Slots::Form).toObject());

  const auto entries = FormData::entry_list(form_obj);
  size_t index = JS::GetReservedSlot(self, Slots::Index).toInt32();
  uint8_t type = JS::GetReservedSlot(self, Slots::Type).toInt32();

  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result)
    return false;

  if (index == entries->length()) {
    JS_DefineProperty(cx, result, "done", JS::TrueHandleValue, JSPROP_ENUMERATE);
    JS_DefineProperty(cx, result, "value", JS::UndefinedHandleValue, JSPROP_ENUMERATE);

    args.rval().setObject(*result);
    return true;
  }

  JS_DefineProperty(cx, result, "done", JS::FalseHandleValue, JSPROP_ENUMERATE);
  auto entry = entries->begin()[index];

  JS::RootedValue result_val(cx);
  JS::RootedValue key_val(cx);
  JS::RootedValue val_val(cx, entry.value);

  if (type != ITER_TYPE_VALUES) {
    JS::RootedString key_str(cx, JS_NewStringCopyZ(cx, entry.name.c_str()));
    if (!key_str) {
      return false;
    }

    key_val = JS::StringValue(key_str);
  }

  switch (type) {
  case ITER_TYPE_ENTRIES: {
    JS::RootedObject pair(cx, JS::NewArrayObject(cx, 2));
    if (!pair)
      return false;
    JS_DefineElement(cx, pair, 0, key_val, JSPROP_ENUMERATE);
    JS_DefineElement(cx, pair, 1, val_val, JSPROP_ENUMERATE);
    result_val = JS::ObjectValue(*pair);
    break;
  }
  case ITER_TYPE_KEYS: {
    result_val = key_val;
    break;
  }
  case ITER_TYPE_VALUES: {
    result_val = val_val;
    break;
  }
  default:
    MOZ_RELEASE_ASSERT(false, "Invalid iter type");
  }

  JS_DefineProperty(cx, result, "value", result_val, JSPROP_ENUMERATE);

  JS::SetReservedSlot(self, Slots::Index, JS::Int32Value(index + 1));
  args.rval().setObject(*result);
  return true;
}

const JSFunctionSpec FormDataIterator::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec FormDataIterator::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec FormDataIterator::methods[] = {
    JS_FN("next", FormDataIterator::next, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec FormDataIterator::properties[] = {
    JS_PS_END,
};

bool FormDataIterator::init_class(JSContext *cx, JS::HandleObject global) {
  JS::RootedObject iterator_proto(cx, JS::GetRealmIteratorPrototype(cx));
  if (!iterator_proto)
    return false;

  if (!init_class_impl(cx, global, iterator_proto))
    return false;

  // Delete both the `FormDataIterator` global property and the
  // `constructor` property on `FormDataIterator.prototype`. The latter
  // because Iterators don't have their own constructor on the prototype.
  return JS_DeleteProperty(cx, global, class_.name) &&
         JS_DeleteProperty(cx, proto_obj, "constructor");
}

JSObject *FormDataIterator::create(JSContext *cx, JS::HandleObject form, uint8_t type) {
  MOZ_RELEASE_ASSERT(type <= ITER_TYPE_VALUES);

  JS::RootedObject self(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!self)
    return nullptr;

  JS::SetReservedSlot(self, Slots::Form, JS::ObjectValue(*form));
  JS::SetReservedSlot(self, Slots::Type, JS::Int32Value(type));
  JS::SetReservedSlot(self, Slots::Index, JS::Int32Value(0));

  return self;
}

const JSFunctionSpec FormData::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec FormData::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec FormData::methods[] = {
    JS_FN("append", append, 0, JSPROP_ENUMERATE),
    JS_FN("delete", remove, 0, JSPROP_ENUMERATE),
    JS_FN("get", get, 0, JSPROP_ENUMERATE),
    JS_FN("getAll", FormData::getAll, 0, JSPROP_ENUMERATE),
    JS_FN("has", FormData::has, 0, JSPROP_ENUMERATE),
    JS_FN("set", FormData::set, 0, JSPROP_ENUMERATE),
    JS_FN("forEach", FormData::forEach, 0, JSPROP_ENUMERATE),
    JS_FN("entries", FormData::entries, 0, JSPROP_ENUMERATE),
    JS_FN("keys", FormData::keys, 0, JSPROP_ENUMERATE),
    JS_FN("values", FormData::values, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec FormData::properties[] = {
    JS_PS_END,
};

// Define entries, keys and values methods
BUILTIN_ITERATOR_METHODS(FormData)

FormData::EntryList* FormData::entry_list(JSObject *self) {
  MOZ_ASSERT(is_instance(self));

  auto entries = static_cast<EntryList *>(
      JS::GetReservedSlot(self, static_cast<size_t>(FormData::Slots::Entries)).toPrivate());

  MOZ_ASSERT(entries);
  return entries;
}

// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#constructing-form-data-set
bool FormData::append(JSContext* cx, HandleObject self, std::string_view key, HandleValue value, HandleValue filename) {
  bool is_file = value.isObject() && File::is_instance(&value.toObject());
  bool is_blob = value.isObject() && Blob::is_instance(&value.toObject());

  auto entries = entry_list(self);

  if (is_file) {
    auto entry = entry_from_kv_pair(key, value);
    entries->append(entry);
  } else if (is_blob) {
    // If value is not a File object, then set value to a new File object,
    // representing the same bytes, whose name attribute value is "blob".
    RootedObject blob(cx, &value.toObject());
    HandleValueArray arr = HandleValueArray(value);

    RootedObject file_bits(cx, NewArrayObject(cx, arr));
    if (!file_bits) {
      return false;
    }

    RootedValue file_bits_val(cx, JS::ObjectValue(*file_bits));
    RootedValue opts_val(cx);
    RootedValue filename_val(cx);

    // Check if type is defined in the current blob, if that't the case
    // prepare options object that contains its type.
    RootedString type(cx, Blob::type(blob));
    if (JS_GetStringLength(type)) {
      RootedObject opts(cx, JS_NewPlainObject(cx));
      if (!opts) {
        return false;
      }

      RootedValue type_val(cx, JS::StringValue(type));
      if (!JS_DefineProperty(cx, opts, "type", type_val, JSPROP_ENUMERATE)) {
        return false;
      }
      opts_val = JS::ObjectValue(*opts);
    }

    if (filename.isNullOrUndefined()) {
      RootedString default_name(cx, JS_NewStringCopyZ(cx, "blob"));
      if (!default_name) {
        return false;
      }

      RootedValue default_name_val(cx, JS::StringValue(default_name));
      filename_val = default_name_val;
    } else {
      filename_val = filename;
    }

    RootedObject file(cx, File::create(cx, file_bits_val, filename_val, opts_val));
    if (!file) {
      return false;
    }

    RootedValue file_val(cx, JS::ObjectValue(*file));
    auto entry = entry_from_kv_pair(key, file_val);
    entries->append(entry);
  } else {
    // If value is a string, then set value to the result of
    // converting value into a scalar value string.
    RootedString str(cx, JS::ToString(cx, value));
    if (!str) {
      return false;
    }

    RootedValue str_val(cx, StringValue(str));
    auto entry = entry_from_kv_pair(key, str_val);
    entries->append(entry);
  }

  return true;
}

bool FormData::append(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(2)

  RootedValue name(cx, args.get(0));
  RootedValue value(cx, args.get(1));
  RootedValue filename(cx, args.get(2));

  HostString name_to_add;
  if (!name_from_args(cx, args, name_to_add)) {
    return false;
  }

  return append(cx, self, name_to_add, value, filename);
}

bool FormData::remove(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  RootedValue name(cx, args.get(0));
  HostString name_to_remove;
  if (!name_from_args(cx, args, name_to_remove)) {
    return false;
  }

  entry_list(self)->eraseIf([&](const FormDataEntry &entry) {
    return entry.name == std::string_view(name_to_remove);
  });

  return true;
}

bool FormData::get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  RootedValue name(cx, args.get(0));
  HostString name_to_get;
  if (!name_from_args(cx, args, name_to_get)) {
    return false;
  }

  auto entries = entry_list(self);
  auto it = std::find_if(entries->begin(), entries->end(), [&](const FormDataEntry &entry) {
    return entry.name == std::string_view(name_to_get);
  });

  if (it != entries->end()) {
    args.rval().set(it->value);
  } else {
    args.rval().setNull();
  }

  return true;
}

bool FormData::getAll(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  HostString name_to_get;
  if (!name_from_args(cx, args, name_to_get)) {
    return false;
  }

  auto entries = entry_list(self);

  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) {
    return false;
  }

  uint32_t index = 0;
  for (auto &entry : *entries) {
    if (entry.name == std::string_view(name_to_get)) {
      JS::RootedValue val(cx, entry.value);
      if (!JS_DefineElement(cx, array, index++, val, JSPROP_ENUMERATE)) {
        return false;
      }
    }
  }

  args.rval().setObject(*array);
  return true;
}

bool FormData::has(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  RootedValue name(cx, args.get(0));

  if (!name.isString()) {
    return false;
  }

  auto name_to_find = core::encode(cx, name);
  if (!name_to_find) {
    return false;
  }

  auto entries = entry_list(self);
  auto it = std::find_if(entries->begin(), entries->end(), [&](const FormDataEntry &entry) {
    return entry.name == std::string_view(name_to_find);
  });

  args.rval().setBoolean(it != entries->end());
  return true;
}

bool FormData::set(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1)

  RootedValue name(cx, args.get(0));
  RootedValue value(cx, args.get(1));
  RootedValue filename(cx, args.get(2));

  HostString name_to_find;
  if (!name_from_args(cx, args, name_to_find)) {
    return false;
  }

  auto entries = entry_list(self);
  auto it = std::find_if(entries->begin(), entries->end(), [&](const FormDataEntry &entry) {
    return entry.name == std::string_view(name_to_find);
  });

  if (it != entries->end()) {
    it->value = value;
    return true;
  } else {
    return append(cx, self, name_to_find, value, filename);
  }
}

bool FormData::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("FormData", 0);

  // The FormData constructor optionally takes HTMLFormElement and HTMLElement as parameters.
  // As we do not support DOM we throw if the first parameter is not undefined.
  //
  // See https://min-common-api.proposal.wintercg.org/#issue-92f53c35
  if (!args.get(0).isUndefined()) {
    return api::throw_error(cx, api::Errors::TypeError, "FormData.constructor", "form", "be undefined");
  }

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));

  if (!self) {
    return false;
  }

  SetReservedSlot(self, static_cast<uint32_t>(Slots::Entries), JS::PrivateValue(new EntryList));

  args.rval().setObject(*self);
  return true;
}

void FormData::finalize(JS::GCContext *gcx, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto entries = entry_list(self);
  if (entries) {
    free(entries);
  }
}

void FormData::trace(JSTracer *trc, JSObject *self) {
  MOZ_ASSERT(is_instance(self));
  auto entries = entry_list(self);
  entries->trace(trc);
}

bool FormData::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
    return false;
  }

  JS::RootedValue entries(cx);
  if (!JS_GetProperty(cx, proto_obj, "entries", &entries)) {
    return false;
  }

  JS::SymbolCode code = JS::SymbolCode::iterator;
  JS::RootedId iteratorId(cx, JS::GetWellKnownSymbolKey(cx, code));
  return JS_DefinePropertyById(cx, proto_obj, iteratorId, entries, 0);
}

bool install(api::Engine *engine) {
  if (!FormData::init_class(engine->cx(), engine->global())) {
    return false;
  }
  if (!FormDataIterator::init_class(engine->cx(), engine->global())) {
    return false;
  }

  return true;
}

} // namespace form_data
} // namespace web
} // namespace builtins
