#include "custom-event.h"



namespace builtins::web::event {

const JSFunctionSpec CustomEvent::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec CustomEvent::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec CustomEvent::methods[] = {
    JS_FS_END,
};

const JSPropertySpec CustomEvent::properties[] = {
    JS_PSG("detail", CustomEvent::detail_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "CustomEvent", JSPROP_READONLY),
    JS_PS_END,
};

bool CustomEvent::detail_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  // TODO: Change this class so that its prototype isn't an instance of the class
  if (self == proto_obj) {
    return api::throw_error(cx, api::Errors::WrongReceiver, "name get", "CustomEvent");
  }

  args.rval().set(JS::GetReservedSlot(self, Slots::Detail));
  return true;
}

// https://dom.spec.whatwg.org/#interface-customevent
bool CustomEvent::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("CustomEvent", 2);

  RootedValue type(cx, args.get(0));
  RootedValue opts(cx, args.get(1));
  RootedValue detail(cx);

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (self == nullptr) {
    return false;
  }

  if (!Event::init(cx, self, type, opts)) {
    return false;
  }

  if (opts.isObject()) {
    JS::RootedObject obj(cx, &opts.toObject());
    if (!JS_GetProperty(cx, obj, "detail", &detail)) {
      return false;
    }
  }

  SetReservedSlot(self, Slots::Detail, detail);

  args.rval().setObject(*self);
  return true;
}

bool CustomEvent::init_class(JSContext *cx, JS::HandleObject global) {
  Event::register_subclass(&class_);
  return init_class_impl(cx, global, Event::proto_obj);
}

} // namespace builtins::web::event


