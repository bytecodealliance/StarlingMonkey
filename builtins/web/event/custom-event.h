#ifndef BUILTINS_WEB_CUSTOM_EVENT_H_
#define BUILTINS_WEB_CUSTOM_EVENT_H_

#include "builtin.h"
#include "event.h"



namespace builtins::web::event {

class CustomEvent : public BuiltinImpl<CustomEvent> {
  static bool detail_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr int ParentSlots = Event::Slots::Count;
  enum Slots { Detail = ParentSlots, Count };

  static constexpr const char *class_name = "CustomEvent";
  static constexpr unsigned ctor_length = 2;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSString *name(JSObject *self);

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

} // namespace builtins::web::event



#endif // BUILTINS_WEB_CUSTOM_EVENT_H_
