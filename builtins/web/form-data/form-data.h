#ifndef BUILTINS_WEB_FORM_FDATA_H
#define BUILTINS_WEB_FORM_FDATA_H

#include "builtin.h"

namespace builtins {
namespace web {
namespace form_data {

struct FormDataEntry {
  FormDataEntry(std::string_view name, HandleValue value) : name(name), value(value) {}

  void trace(JSTracer *trc) { TraceEdge(trc, &value, "FormDataEntry value"); }

  std::string name;
  JS::Heap<JS::Value> value;
};


class FormDataIterator : public BuiltinNoConstructor<FormDataIterator> {
public:
  static constexpr const char *class_name = "FormDataIterator";

  enum Slots { Form, Type, Index, Count };

  static bool next(JSContext *cx, unsigned argc, JS::Value *vp);

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static const unsigned ctor_length = 0;

  static JSObject *create(JSContext *cx, JS::HandleObject params, uint8_t type);
  static bool init_class(JSContext *cx, JS::HandleObject global);
};

class FormData : public BuiltinImpl<FormData, TraceableClassPolicy> {
  static bool append(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool remove(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool getAll(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool has(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool set(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool forEach(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool entries(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool keys(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool values(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool append(JSContext *cx, HandleObject self, std::string_view key, HandleValue val,
                     HandleValue filename);

  using EntryList = JS::GCVector<FormDataEntry, 0, js::SystemAllocPolicy>;
  static EntryList *entry_list(JSObject *self);

  friend class UrlParser;
  friend class FormDataIterator;
  friend class MultipartParser;
  friend class MultipartFormData;

public:
  static constexpr const char *class_name = "FormData";

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static constexpr unsigned ctor_length = 0;

  enum Slots { Entries, Count };

  static JSObject *create(JSContext *cx);
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
