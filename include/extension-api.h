#ifndef EXTENSION_API_H
#define EXTENSION_API_H
#include <vector>

#include "builtin.h"

// TODO: remove these once the warnings are fixed
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"

#include "jsapi.h"
#pragma clang diagnostic pop

using JS::RootedObject;
using JS::RootedString;
using JS::RootedValue;

using JS::HandleObject;
using JS::HandleValue;
using JS::HandleValueArray;
using JS::MutableHandleValue;

using JS::PersistentRooted;
using JS::PersistentRootedVector;

using std::optional;

namespace api {

typedef int32_t PollableHandle;
constexpr PollableHandle INVALID_POLLABLE_HANDLE = -1;
constexpr PollableHandle IMMEDIATE_TASK_HANDLE = -2;

class AsyncTask;

struct EngineConfig {
  mozilla::Maybe<std::string> content_script_path = mozilla::Nothing();
  mozilla::Maybe<std::string> content_script = mozilla::Nothing();
  mozilla::Maybe<std::string> path_prefix = mozilla::Nothing();
  bool module_mode = true;

  /**
   * Path to the script to evaluate before the content script.
   *
   * This script is evaluated in a separate global and has access to functions not
   * available to content. It can be used to set up the environment for the content
   * script, e.g. by registering builtin modules or adding global properties.
   */
  mozilla::Maybe<std::string> initializer_script_path = mozilla::Nothing();

  /**
   * Whether to evaluate the top-level script in pre-initialization mode or not.
   *
   * During pre-initialization, functionality that depends on WASIp2 is unavailable.
   */
  bool pre_initialize = false;
  bool verbose = false;

  /**
   * Whether to enable the script debugger. If this is enabled, the runtime will
   * check for the DEBUGGER_PORT environment variable and try to connect to that
   * port on localhost if it's set. If that succeeds, it expects the host to send
   * a script to use as the debugger, using the SpiderMonkey Debugger API.
   */
  bool debugging = false;

  /**
   * Whether to enable Web Platform Test mode. Specifically, this means installing a
   * few global properties required to make WPT work, that wouldn't be made available
   * to content.
   */
  bool wpt_mode = false;

  EngineConfig() = default;
};

enum class EngineState { Uninitialized, EngineInitializing, ScriptPreInitializing, Initialized, Aborted };

class Engine {
  std::unique_ptr<EngineConfig> config_;
  EngineState state_ = EngineState::Uninitialized;

public:
  explicit Engine(std::unique_ptr<EngineConfig> config);
  static Engine *get(JSContext *cx);

  JSContext *cx();
  JS::HandleObject global();
  EngineState state();
  bool debugging_enabled();
  bool wpt_mode();

  void finish_pre_initialization();

  /**
   * Define a new builtin module
   *
   * The enumerable properties of the builtin object are used to construct
   * a synthetic module namespace for the module.
   *
   * The enumeration and getters are called only on the first import of
   * the builtin, so that lazy getters can be used to lazily initialize
   * builtins.
   *
   * Once loaded, the instance is cached and reused as a singleton.
   */
  bool define_builtin_module(const char *id, JS::HandleValue builtin);

  /**
   * Treat the top-level script as a module or classic JS script.
   *
   * By default, the engine treats the top-level script as a module.
   * Since not all content can be run as a module, this method allows
   * changing this default, and will impact all subsequent top-level
   * evaluations.
   */
  bool eval_toplevel(const char *path, MutableHandleValue result);
  bool eval_toplevel(JS::SourceText<mozilla::Utf8Unit> &source, const char *path,
                     MutableHandleValue result);

  /**
   * Run the script set using the `-i | --initializer-script-path` option.
   *
   * This script runs in a separate global, and has access to functions not
   * available to content. Notably, that includes the ability to define
   * builtin modules, using the `defineBuiltinModule` function.
   */
  bool run_initialization_script();

  /**
   * Returns the global the initialization script runs in.
   */
  HandleObject init_script_global();

  /**
   * Run the async event loop as long as there's interest registered in keeping it running.
   *
   * Each turn of the event loop consists of three steps:
   * 1. Run reactions to all promises that have been resolves/rejected.
   * 2. Check if there's any interest registered in continuing to wait for async tasks, and
   *    terminate the loop if not.
   * 3. Wait for the next async tasks and execute their reactions
   *
   * Interest or loss of interest in keeping the event loop running can be signaled using the
   * `Engine::incr_event_loop_interest` and `Engine::decr_event_loop_interest` methods.
   *
   * Every call to incr_event_loop_interest must be followed by an eventual call to
   * decr_event_loop_interest, for the event loop to complete. Otherwise, if no async tasks remain
   * pending while there's still interest in the event loop, an error will be reported.
   */
  bool run_event_loop();

  /**
   * Add an event loop interest to track
   */
  void incr_event_loop_interest();

  /**
   * Remove an event loop interest to track
   * The last decrementer marks the event loop as complete to finish
   */
  void decr_event_loop_interest();

  /**
   * Get the JS value associated with the top-level script execution -
   * the last expression for a script, or the module namespace for a module.
   */
  JS::HandleValue script_value();

  bool has_pending_async_tasks();
  void queue_async_task(AsyncTask *task);
  bool cancel_async_task(AsyncTask *task);

  bool has_unhandled_promise_rejections();
  void report_unhandled_promise_rejections();
  void clear_unhandled_promise_rejections();

  void abort(const char *reason);

  bool debug_logging_enabled();

  bool dump_value(JS::Value val, FILE *fp = stdout);
  bool print_stack(FILE *fp);
  void dump_error(JS::HandleValue error, FILE *fp = stderr);
  void dump_pending_exception(const char *description = "", FILE *fp = stderr);
  void dump_promise_rejection(JS::HandleValue reason, JS::HandleObject promise, FILE *fp = stderr);
};


typedef bool (*TaskCompletionCallback)(JSContext* cx, JS::HandleObject receiver);

class AsyncTask {
protected:
  PollableHandle handle_ = -1;

public:
  virtual ~AsyncTask() = default;

  virtual bool run(Engine *engine) = 0;
  virtual bool cancel(Engine *engine) = 0;

  [[nodiscard]] virtual PollableHandle id() {
    MOZ_ASSERT(handle_ != INVALID_POLLABLE_HANDLE);
    return handle_;
  }

  [[nodiscard]] virtual uint64_t deadline() {
    return 0;
  }

  virtual void trace(JSTracer *trc) = 0;

  /**
   * Select for the next available ready task, providing the oldest ready first.
   */
  static size_t select(std::vector<AsyncTask *> &handles);
};

} // namespace api

#endif // EXTENSION_API_H
