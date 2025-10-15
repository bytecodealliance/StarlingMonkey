#include "worker-location.h"
#include "url.h"

/**
 * The `WorkerLocation` builtin, added to the global object as the data property
 * `location`.
 * https://html.spec.whatwg.org/multipage/workers.html#worker-locations
 */


namespace builtins::web::worker_location {

JS::PersistentRooted<JSObject *> WorkerLocation::url;

namespace {
DEF_ERR(LocationNotSetError, JSEXN_TYPEERR, "{0} can only be used during request handling, "
                                            "or if an initialization-time location was set "
                                            "using `--init-location`", 1)
bool ensure_location_access(JSContext *cx, const char *name) {
  auto *engine = api::Engine::get(cx);

  if (engine->state() == api::EngineState::Initialized) {
    return true;
  }

  if (engine->state() == api::EngineState::ScriptPreInitializing && WorkerLocation::url.get()) {
    return true;
  }

  return api::throw_error(cx, LocationNotSetError, name);
}
} // namespace

#define WorkerLocation_ACCESSOR_GET(field)                                                         \
  bool field##_get(JSContext *cx, unsigned argc, JS::Value *vp) {                                  \
    auto result = WorkerLocation::MethodHeaderWithName(0, cx, argc, vp, __func__);                 \
    if (result.isErr()) {                                                                          \
      return false;                                                                                \
    }                                                                                              \
    auto [args, self] = result.unwrap();                                                           \
    if (!ensure_location_access(cx, "location." #field)) {                                         \
      return false;                                                                                \
    }                                                                                              \
    return url::URL::field(cx, WorkerLocation::url, args.rval());                                  \
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

bool install(api::Engine *engine) {
  if (!WorkerLocation::init_class(engine->cx(), engine->global())) {
    return false;
  }

  const auto &init_location = engine->init_location();
  if (init_location) {
    // Set the URL for `globalThis.location` to the configured value.
    JSContext *cx = engine->cx();
    JS::RootedObject url_instance(
        cx, JS_NewObjectWithGivenProto(cx, &url::URL::class_, url::URL::proto_obj));
    if (!url_instance) {
      return false;
    }

    auto *uri_bytes = new uint8_t[init_location->size() + 1];
    std::copy(init_location->begin(), init_location->end(), uri_bytes);
    jsurl::SpecString spec(uri_bytes, init_location->size(), init_location->size());

    WorkerLocation::url = url::URL::create(cx, url_instance, spec);
  }

  return true;
}

} // namespace builtins::web::worker_location


