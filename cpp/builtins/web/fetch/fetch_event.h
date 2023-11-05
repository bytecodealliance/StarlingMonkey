#ifndef BUILTINS_FETCH_EVENT_H
#define BUILTINS_FETCH_EVENT_H

#include "builtins/builtin.h"
#include "engine.h"
#include "host_api.h"

namespace builtins {
namespace web {
namespace fetch_event {

class FetchEvent final : public BuiltinNoConstructor<FetchEvent> {
  static bool respondWith(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool request_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool waitUntil(JSContext *cx, unsigned argc, JS::Value *vp);
#ifdef CAE
  static bool client_get(JSContext *cx, unsigned argc, JS::Value *vp);
#endif

public:
  static constexpr const char *class_name = "FetchEvent";

  enum class State {
    unhandled,
    waitToRespond,
    responseStreaming,
    responseDone,
    respondedWithError,
  };

  enum class Slots {
    Dispatch,
    Request,
    State,
    PendingPromiseCount,
    DecPendingPromiseCountFunc,
    ClientInfo,
    Count
  };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSObject *create(JSContext *cx);

  /**
   * Create a Request object for the incoming request.
   *
   * Since this happens during initialization time, the object will not be fully
   * initialized. It's filled in at runtime using `init_incoming_request`.
   */
  static JSObject *prepare_downstream_request(JSContext *cx);

  /**
   * Fully initialize the Request object based on the incoming request.
   */
  static bool init_incoming_request(JSContext *cx, JS::HandleObject self,
                                    host_api::HttpIncomingRequest* req);

  static bool respondWithError(JSContext *cx, JS::HandleObject self);
  static bool is_active(JSObject *self);
  static bool is_dispatching(JSObject *self);
  static void start_dispatching(JSObject *self);
  static void stop_dispatching(JSObject *self);

  static State state(JSObject *self);
  static void set_state(JSObject *self, State state);
  static bool response_started(JSObject *self);

  static JS::HandleObject instance();
};

bool install(core::Engine* engine);

} // namespace fetch_event
} // namespace web
} // namespace builtins

#endif
