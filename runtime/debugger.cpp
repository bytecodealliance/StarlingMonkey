#include <debugger.h>

#include <decode.h>
#include <encode.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <string_view>

mozilla::Maybe<std::string> main_path;

namespace SocketErrors {
DEF_ERR(SendFailed, JSEXN_TYPEERR, "Failed to send message via TCP socket", 0)
} // namespace SocketErrors

static bool dbg_set_content_path(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  auto path = core::encode(cx, args.get(0));
  if (!path) {
    return false;
  }

  main_path = mozilla::Some(path.ptr.release());
  args.rval().setUndefined();
  return true;
}

static bool print_location(JSContext *cx, FILE *fp = stdout) {
  JS::AutoFilename filename;
  uint32_t lineno;
  JS::ColumnNumberOneOrigin column;
  if (!DescribeScriptedCaller(cx, &filename, &lineno, &column)) {
    return false;
  }
  fprintf(fp, "%s@%u:%u: ", filename.get(), lineno, column.oneOriginValue());
  return true;
}

static bool dbg_print(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!print_location(cx)) {
    return false;
  }

  for (size_t i = 0; i < args.length(); i++) {
    auto str = core::encode(cx, args.get(i));
    if (!str) {
      return false;
    }
    printf("%.*s", static_cast<int>(str.len), str.begin());
  }

  printf("\n");
  fflush(stdout);
  args.rval().setUndefined();
  return true;
}

static bool dbg_assert(JSContext *cx, unsigned argc, Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!ToBoolean(args.get(0))) {

    if (!print_location(cx, stderr)) {
      return false;
    }

    if (args.length() > 1) {
      auto message = core::encode(cx, args.get(1));
      fprintf(stderr, "Assert failed in debugger: %.*s",
        static_cast<int>(message.len), message.begin());
    } else {
      fprintf(stderr, "Assert failed in debugger");
    }
    MOZ_ASSERT(false);
  }
  args.rval().setUndefined();
  return true;
}

#include "sockets.h"

namespace debugging_socket {

class TCPSocket : public builtins::BuiltinNoConstructor<TCPSocket> {
  static host_api::TCPSocket *socket(JSObject *self);
  static bool send(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool receive(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  static constexpr const char *class_name = "TCPSocket";
  enum Slots { TCPSocketHandle, Count };

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static JSObject *FromSocket(JSContext *cx, host_api::TCPSocket *socket);
};

host_api::TCPSocket *TCPSocket::socket(JSObject *self) {
  auto socket_val = JS::GetReservedSlot(self, TCPSocketHandle);
  return static_cast<host_api::TCPSocket *>(socket_val.toPrivate());
}

bool TCPSocket::send(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);
  auto chunk = core::encode(cx, args[0]);
  if (!socket(self)->send(std::move(chunk))) {
    return api::throw_error(cx, SocketErrors::SendFailed);
  }
  return true;
}

bool TCPSocket::receive(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(1);
  int32_t chunk_size;
  if (!ToInt32(cx, args[0], &chunk_size)) {
    return false;
  }
  auto chunk = socket(self)->receive(chunk_size);
  auto str = core::decode(cx, chunk);
  if (!str) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

JSObject* TCPSocket::FromSocket(JSContext* cx, host_api::TCPSocket* socket) {
  RootedObject instance(cx, JS_NewObjectWithGivenProto(cx, &class_, proto_obj));
  if (!instance) {
    return nullptr;
  }
  SetReservedSlot(instance, TCPSocketHandle, PrivateValue(socket));
  return instance;
}

const JSFunctionSpec TCPSocket::methods[] = {
  JS_FN("send", TCPSocket::send, 1, 0),
  JS_FN("receive", TCPSocket::receive, 1, 0), JS_FS_END
};

const JSFunctionSpec TCPSocket::static_methods[] = {JS_FS_END};
const JSPropertySpec TCPSocket::static_properties[] = {JS_PS_END};
const JSPropertySpec TCPSocket::properties[] = {JS_PS_END};

host_api::HostString read_message(JSContext *cx, host_api::TCPSocket *socket) {
  auto chunk = socket->receive(128);
  if (!chunk) {
    return nullptr;
  }
  char *end;
  uint16_t message_length = std::strtoul(chunk.begin(), &end, 10);
  if (end == chunk.begin() || *end != '\n') {
    return nullptr;
  }
  std::string message = std::string(end + 1, chunk.end());
  while (message.size() < message_length) {
    chunk = socket->receive(message_length - message.size());
    if (!chunk) {
      return nullptr;
    }
    message.append(chunk.begin(), chunk.end());
  }
  return {std::move(message)};
}

} // namespace debugging_socket

bool initialize_debugger(JSContext *cx, uint16_t port, bool content_already_initialized) {
  auto socket = host_api::TCPSocket::make(host_api::TCPSocket::IPAddressFamily::IPV4);
  MOZ_RELEASE_ASSERT(socket, "Failed to create debugging socket");
  if (!socket->connect({127, 0, 0, 1}, port) || !socket->send("get-session-port")) {
    printf("Couldn't connect to debugging socket at port %u, continuing without debugging ...\n",
           port);
    return true;
  }
  auto response = socket->receive(128);
  if (!response) {
    printf("Couldn't get debugging session port, continuing without debugging ...\n");
    return true;
  }
  char *end;

  // If StarlingMonkey was loaded with debugging enabled, but no session is active,
  // we can just silently continue execution.
  if (string_view{"no-session"}.compare(0, response.len, response) == 0) {
    return true;
  }

  uint16_t session_port = std::strtoul(response.begin(), &end, 10);
  if (end != response.end() || session_port > 65535) {
    printf("Invalid debugging session port '%*s' received, continuing without debugging ...\n",
           static_cast<int>(response.len), response.begin());
    return true;
  }
  socket->close();

  socket = host_api::TCPSocket::make(host_api::TCPSocket::IPAddressFamily::IPV4);
  MOZ_RELEASE_ASSERT(socket, "Failed to create debugging session socket");
  if (!socket->connect({127, 0, 0, 1}, session_port) || !socket->send("get-debugger")) {
    printf("Couldn't connect to debugging session socket at port %u, "
           "continuing without debugging ...\n",
           session_port);
    return true;
  }
  auto debugging_script = debugging_socket::read_message(cx, socket);
  if (!debugging_script) {
    printf("Couldn't get debugger script, continuing without debugging ...\n");
    return true;
  }

  JS::RealmOptions options;
  options.creationOptions()
      .setStreamsEnabled(true)
      .setNewCompartmentInSystemZone()
      .setInvisibleToDebugger(true);

  static JSClass global_class = {"global", JSCLASS_GLOBAL_FLAGS, &JS::DefaultGlobalClassOps};
  RootedObject global(cx);
  global = JS_NewGlobalObject(cx, &global_class, nullptr, JS::DontFireOnNewGlobalHook, options);
  if (!global) {
    return false;
  }

  JSAutoRealm ar(cx, global);

  if (!JS_DefineDebuggerObject(cx, global)) {
    return false;
  }

  if (!JS_DefineFunction(cx, global, "setContentPath", dbg_set_content_path, 1, 0) ||
      !JS_DefineFunction(cx, global, "print", dbg_print, 1, 0) ||
      !JS_DefineFunction(cx, global, "assert", dbg_assert, 1, 0)) {
    return false;
  }

  if (!debugging_socket::TCPSocket::init_class_impl(cx, global)) {
    return false;
  }

  RootedObject socket_obj(cx, debugging_socket::TCPSocket::FromSocket(cx, socket));
  if (!socket_obj) {
    return false;
  }
  if (!JS_DefineProperty(cx, global, "socket", socket_obj, JSPROP_READONLY)) {
    return false;
  }

  RootedValue val(cx, JS::BooleanValue(content_already_initialized));
  if (!JS_DefineProperty(cx, global, "contentAlreadyInitialized", val, JSPROP_READONLY)) {
    return false;
  }

  JS::SourceText<mozilla::Utf8Unit> source;
  if (!source.init(cx, std::move(debugging_script.ptr), debugging_script.len)) {
    return false;
  }

  JS::CompileOptions opts(cx);
  opts.setFile("<debugger>");
  JS::RootedScript script(cx, JS::Compile(cx, opts, source));
  if (!script) {
    return false;
  }
  RootedValue result(cx);
  if (!JS_ExecuteScript(cx, script, &result)) {
    return false;
  }

  return true;
}

static bool debugger_initialized = false;
void content_debugger::maybe_init_debugger(api::Engine * engine, bool content_already_initialized) {
  if (debugger_initialized || !engine->debugging_enabled()) {
    return;
  }
  debugger_initialized = true;
  auto port_str = std::getenv("DEBUGGER_PORT");
  if (port_str) {
    uint32_t port = std::stoi(port_str);
    if (!initialize_debugger(engine->cx(), port, content_already_initialized)) {
      fprintf(stderr, "Error evaluating debugger script\n");
      exit(1);
    }
  }
}

mozilla::Maybe<std::string_view> content_debugger::replacement_script_path() {
  return main_path;
}
