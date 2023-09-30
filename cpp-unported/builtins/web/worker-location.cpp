#include "builtins/worker-location.h"
#include "builtin.h"
#include "builtins/shared/url.h"
#include "js-compute-builtins.h"

/**
 * The `WorkerLocation` builtin, added to the global object as the data property
 * `location`.
 * https://html.spec.whatwg.org/multipage/workers.html#worker-locations
 */
namespace builtins {
JS::PersistentRooted<JSObject *> WorkerLocation::url;

#define WorkerLocation_ACCESSOR_GET(field)                                                         \
  bool field##_get(JSContext *cx, unsigned argc, JS::Value *vp) {                                  \
    auto result = WorkerLocation::MethodHeaderWithName(0, cx, argc, vp, __func__);                 \
    if (result.isErr()) {                                                                          \
      return false;                                                                                \
    }                                                                                              \
    auto [args, self] = result.unwrap();                                                           \
    REQUEST_HANDLER_ONLY("location." #field)                                                       \
    return URL::field(cx, WorkerLocation::url, args.rval());                                       \
  }

WorkerLocation_ACCESSOR_GET(href);
WorkerLocation_ACCESSOR_GET(origin);
WorkerLocation_ACCESSOR_GET(protocol);
WorkerLocation_ACCESSOR_GET(host);
WorkerLocation_ACCESSOR_GET(hostname);
WorkerLocation_ACCESSOR_GET(port);
WorkerLocation_ACCESSOR_GET(pathname);
WorkerLocation_ACCESSOR_GET(search);
WorkerLocation_ACCESSOR_GET(hash);

#undef WorkerLocation_ACCESSOR_GET

bool WorkerLocation::toString(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0)
  return href_get(cx, argc, vp);
}

const JSFunctionSpec WorkerLocation::static_methods[] = {
    JS_FS_END,
};

const JSPropertySpec WorkerLocation::static_properties[] = {
    JS_PS_END,
};

const JSFunctionSpec WorkerLocation::methods[] = {JS_FN("toString", toString, 0, JSPROP_ENUMERATE),
                                                  JS_FS_END};

const JSPropertySpec WorkerLocation::properties[] = {
    JS_PSG("href", href_get, JSPROP_ENUMERATE),
    JS_PSG("origin", origin_get, JSPROP_ENUMERATE),
    JS_PSG("protocol", protocol_get, JSPROP_ENUMERATE),
    JS_PSG("host", host_get, JSPROP_ENUMERATE),
    JS_PSG("hostname", hostname_get, JSPROP_ENUMERATE),
    JS_PSG("port", port_get, JSPROP_ENUMERATE),
    JS_PSG("pathname", pathname_get, JSPROP_ENUMERATE),
    JS_PSG("search", search_get, JSPROP_ENUMERATE),
    JS_PSG("hash", hash_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "Location", JSPROP_READONLY),
    JS_PS_END};

bool WorkerLocation::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  JS_ReportErrorLatin1(cx, "Illegal constructor WorkerLocation");
  return false;
}

bool WorkerLocation::init_class(JSContext *cx, JS::HandleObject global) {
  if (!init_class_impl(cx, global)) {
    return false;
  }

  WorkerLocation::url.init(cx);

  JS::RootedObject location(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!location) {
    return false;
  }

  return JS_DefineProperty(cx, global, "location", location, JSPROP_ENUMERATE);
}

} // namespace builtins
