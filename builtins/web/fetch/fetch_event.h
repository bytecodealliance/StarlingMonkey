#ifndef BUILTINS_WEB_FETCH_FETCH_EVENT_H
#define BUILTINS_WEB_FETCH_FETCH_EVENT_H

#include "builtin.h"
#include "extension-api.h"
#include "host_api.h"

#include "../event/event.h"

namespace builtins::web::fetch::fetch_event {

class FetchEvent final : public BuiltinNoConstructor<FetchEvent> {
  static bool respondWith(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool request_get(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool waitUntil(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "FetchEvent";

  enum class State {
    unhandled,
    waitToRespond,
    responseStreaming,
    responseDone,
    respondedWithError,
  };

  static constexpr int ParentSlots = event::Event::Slots::Count;

  enum Slots {
    Request = ParentSlots,
    CurrentState,
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
                                    host_api::HttpIncomingRequest *req);

  static bool respondWithError(JSContext *cx, JS::HandleObject self);
  static bool respondWithErrorString(JSContext *cx, JS::HandleObject self, std::string_view body_text);
  static bool is_active(JSObject *self);

  static State state(JSObject *self);
  static void set_state(JSObject *self, State state);
  static bool response_started(JSObject *self);

  static JS::HandleObject instance();

  static void increase_interest();
  static void decrease_interest();

  static bool init_class(JSContext *cx, HandleObject global);
};

bool install(api::Engine *engine);

} // namespace builtins::web::fetch::fetch_event

#endif
