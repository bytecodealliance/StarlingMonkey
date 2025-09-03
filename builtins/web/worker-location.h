#ifndef BUILTINS_WEB_WORKER_LOCATION_H
#define BUILTINS_WEB_WORKER_LOCATION_H

#include "builtin.h"



namespace builtins::web::worker_location {

class WorkerLocation : public BuiltinNoConstructor<WorkerLocation> {
private:
public:
  static constexpr const char *class_name = "WorkerLocation";
  static const int ctor_length = 1;
  enum Slots : uint8_t { Count };
  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JS::PersistentRooted<JSObject *> url;
  static bool toString(JSContext *cx, unsigned argc, JS::Value *vp);

  static bool init_class(JSContext *cx, JS::HandleObject global);
};

bool install(api::Engine *engine);

} // namespace builtins::web::worker_location



#endif
