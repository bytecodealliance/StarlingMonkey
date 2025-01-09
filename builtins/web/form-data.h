#ifndef BUILTINS_WEB_FORM_FDATA_H
#define BUILTINS_WEB_FORM_FDATA_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace form_data {

struct FormDataEntry {
  std::string name;
  JS::Heap<JS::Value> value;

  void trace(JSTracer *trc) { TraceEdge(trc, &value, "FormDataEntry value"); }
};

class FormData : public TraceableBuiltinImpl<FormData> {
  static bool append(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool remove(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool getAll(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool has(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool set(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool append(JSContext *cx, HandleObject self, std::string_view key, HandleValue val,
                     HandleValue filename);

  using EntryList = JS::GCVector<FormDataEntry, 0, js::SystemAllocPolicy>;
  static EntryList *entry_list(JSObject *self);

public:
  static constexpr const char *class_name = "FormData";

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr unsigned ctor_length = 0;

  enum Slots { Entries, Count };

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
  static void finalize(JS::GCContext *gcx, JSObject *self);
  static void trace(JSTracer *trc, JSObject *self);
};

bool install(api::Engine *engine);

} // namespace form_data
} // namespace web
} // namespace builtins

#endif // BUILTINS_WEB_FORM_FDATA_H
