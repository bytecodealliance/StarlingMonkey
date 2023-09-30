#include "dom-exception.h"
#include "builtin.h"
#include "core/encode.h"
#include "js-compute-builtins.h"
#include "js/Context.h"

namespace builtins {

bool DOMException::name_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  if (self == proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSBuiltinErrNum::JSMSG_INVALID_INTERFACE, "name get", "DOMException");
    return false;
  }
  args.rval().setString(JS::GetReservedSlot(self, Slots::Name).toString());
  return true;
}

bool DOMException::message_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  if (self == proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSBuiltinErrNum::JSMSG_INVALID_INTERFACE, "name get", "DOMException");
    return false;
  }
  args.rval().setString(JS::GetReservedSlot(self, Slots::Message).toString());
  return true;
}

bool DOMException::code_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  if (self == proto_obj) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSBuiltinErrNum::JSMSG_INVALID_INTERFACE, "name get", "DOMException");
    return false;
  }
  JS::RootedString name_string(cx, JS::GetReservedSlot(self, Slots::Name).toString());
  auto chars = core::encode(cx, name_string);
  if (!chars) {
    return false;
  }
  std::string_view name(chars.begin(), chars.len);
  int32_t code = 0;
  if (name == "IndexSizeError") {
    code = 1;
  } else if (name == "HierarchyRequestError") {
    code = 3;
  } else if (name == "WrongDocumentError") {
    code = 4;
  } else if (name == "InvalidCharacterError") {
    code = 5;
  } else if (name == "NoModificationAllowedError") {
    code = 7;
  } else if (name == "NotFoundError") {
    code = 8;
  } else if (name == "NotSupportedError") {
    code = 9;
  } else if (name == "InUseAttributeError") {
    code = 10;
  } else if (name == "InvalidStateError") {
    code = 11;
  } else if (name == "SyntaxError") {
    code = 12;
  } else if (name == "InvalidModificationError") {
    code = 13;
  } else if (name == "NamespaceError") {
    code = 14;
  } else if (name == "InvalidAccessError") {
    code = 15;
  } else if (name == "TypeMismatchError") {
    code = 17;
  } else if (name == "SecurityError") {
    code = 18;
  } else if (name == "NetworkError") {
    code = 19;
  } else if (name == "AbortError") {
    code = 20;
  } else if (name == "URLMismatchError") {
    code = 21;
  } else if (name == "QuotaExceededError") {
    code = 22;
  } else if (name == "TimeoutError") {
    code = 23;
  } else if (name == "InvalidNodeTypeError") {
    code = 24;
  } else if (name == "DataCloneError") {
    code = 25;
  }

  args.rval().setInt32(code);
  return true;
}

const JSFunctionSpec DOMException::methods[] = {JS_FS_END};
const JSFunctionSpec DOMException::static_methods[] = {JS_FS_END};

const JSPropertySpec DOMException::properties[] = {
    JS_PSG("code", code_get, JSPROP_ENUMERATE),
    JS_PSG("message", message_get, JSPROP_ENUMERATE),
    JS_PSG("name", name_get, JSPROP_ENUMERATE),
    JS_INT32_PS("INDEX_SIZE_ERR", 1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("DOMSTRING_SIZE_ERR", 2, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("HIERARCHY_REQUEST_ERR", 3, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("WRONG_DOCUMENT_ERR", 4, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_CHARACTER_ERR", 5, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NO_DATA_ALLOWED_ERR", 6, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NO_MODIFICATION_ALLOWED_ERR", 7,
                JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NOT_FOUND_ERR", 8, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NOT_SUPPORTED_ERR", 9, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INUSE_ATTRIBUTE_ERR", 10, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_STATE_ERR", 11, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("SYNTAX_ERR", 12, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_MODIFICATION_ERR", 13,
                JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NAMESPACE_ERR", 14, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_ACCESS_ERR", 15, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("VALIDATION_ERR", 16, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("TYPE_MISMATCH_ERR", 17, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("SECURITY_ERR", 18, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NETWORK_ERR", 19, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("ABORT_ERR", 20, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("URL_MISMATCH_ERR", 21, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("QUOTA_EXCEEDED_ERR", 22, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("TIMEOUT_ERR", 23, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_NODE_TYPE_ERR", 24, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("DATA_CLONE_ERR", 25, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_STRING_SYM_PS(toStringTag, "DOMException",
                     JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_PS_END};
const JSPropertySpec DOMException::static_properties[] = {
    JS_INT32_PS("INDEX_SIZE_ERR", 1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("DOMSTRING_SIZE_ERR", 2, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("HIERARCHY_REQUEST_ERR", 3, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("WRONG_DOCUMENT_ERR", 4, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_CHARACTER_ERR", 5, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NO_DATA_ALLOWED_ERR", 6, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NO_MODIFICATION_ALLOWED_ERR", 7,
                JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NOT_FOUND_ERR", 8, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NOT_SUPPORTED_ERR", 9, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INUSE_ATTRIBUTE_ERR", 10, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_STATE_ERR", 11, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("SYNTAX_ERR", 12, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_MODIFICATION_ERR", 13,
                JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NAMESPACE_ERR", 14, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_ACCESS_ERR", 15, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("VALIDATION_ERR", 16, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("TYPE_MISMATCH_ERR", 17, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("SECURITY_ERR", 18, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("NETWORK_ERR", 19, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("ABORT_ERR", 20, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("URL_MISMATCH_ERR", 21, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("QUOTA_EXCEEDED_ERR", 22, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("TIMEOUT_ERR", 23, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("INVALID_NODE_TYPE_ERR", 24, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_INT32_PS("DATA_CLONE_ERR", 25, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT),
    JS_PS_END};

JSObject *DOMException::create(JSContext *cx, std::string_view message, std::string_view name) {
  JS::RootedValueArray<2> args(cx);
  args[0].setString(JS_NewStringCopyN(cx, message.data(), message.size()));
  args[1].setString(JS_NewStringCopyN(cx, name.data(), name.size()));
  JS::RootedObject instance(cx);
  JS::RootedObject ctorObj(cx, JS_GetConstructor(cx, proto_obj));
  JS::RootedValue ctor(cx, JS::ObjectValue(*ctorObj));
  if (!JS::Construct(cx, ctor, args, &instance)) {
    return nullptr;
  }
  if (!instance) {
    return nullptr;
  }
  auto message_str = JS_NewStringCopyN(cx, message.data(), message.size());
  if (!message_str) {
    return nullptr;
  }
  JS::SetReservedSlot(instance, Slots::Message, JS::StringValue(message_str));
  auto name_str = JS_NewStringCopyN(cx, name.data(), name.size());
  if (!name_str) {
    return nullptr;
  }
  JS::SetReservedSlot(instance, Slots::Name, JS::StringValue(name_str));

  return instance;
}

void DOMException::raise(JSContext *cx, std::string_view message, std::string_view name) {
  JS::RootedObject errorObj(cx);
  errorObj.set(DOMException::create(cx, message, name));
  JS::RootedValue er(cx, JS::ObjectValue(*errorObj));
  JS_SetPendingException(cx, er);
}

// constructor(optional DOMString message = "", optional DOMString name = "Error");
bool DOMException::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("DOMException", 0);

  JS::RootedObject instance(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!instance) {
    return false;
  }
  if (args.hasDefined(0)) {
    auto message = JS::ToString(cx, args.get(0));
    if (!message) {
      return false;
    }
    JS::SetReservedSlot(instance, Slots::Message, JS::StringValue(message));
  } else {
    JS::SetReservedSlot(instance, Slots::Message, JS_GetEmptyStringValue(cx));
  }
  if (args.hasDefined(1)) {
    auto name = JS::ToString(cx, args.get(1));
    if (!name) {
      return false;
    }
    JS::SetReservedSlot(instance, Slots::Name, JS::StringValue(name));
  } else {
    JS::SetReservedSlot(instance, Slots::Name, JS::StringValue(JS_NewStringCopyZ(cx, "Error")));
  }

  args.rval().setObject(*instance);
  return true;
}

bool DOMException::init_class(JSContext *cx, JS::HandleObject global) {
  JS::RootedObject proto(cx, JS::GetRealmErrorPrototype(cx));
  if (!proto) {
    return false;
  }

  return init_class_impl(cx, global, proto);
}

} // namespace builtins
