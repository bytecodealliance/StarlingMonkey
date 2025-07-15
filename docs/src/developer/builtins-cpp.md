# Devoloping builtins in C++

## Adding custom builtins

Adding builtins is as simple as calling `add_builtin` in the importing project's `CMakeLists.txt`.
Say you want to add a builtin defined in the file `my-builtin.cpp`, like so:

```cpp
// The extension API is automatically on the include path for builtins.
#include "extension-api.h"

// The namespace name must match the name passed to `add_builtin` in the CMakeLists.txt
namespace my_project::my_builtin {
  bool install(api::Engine* engine) {
    printf("installing my-builtin\n");
    return true;
  }
} // namespace my_builtin
```

This file can now be included in the runtime's builtins like so:

```cmake
add_builtin(my_project::my_builtin SRC my-builtin.cpp)
```

If your builtin requires multiple `.cpp` files, you can pass all of them to `add_builtin` as values
for the `SRC` argument.

## Writing builtins

### Rooting

> SpiderMonkey has a moving GC, it is very important that it knows about each and every pointer to a
> GC thing in the system. SpiderMonkey's rooting API tries to make this task as simple as possible.

It is highly recommended that readers review the `SpiderMonkey`
[documentation][spidermonkey-rooting] on rooting before proceeding with this section. For
convenience, here is a brief recap of the main rooting rules from the referenced page:

- Use `JS::Rooted<T>` typedefs for local variables on the stack.
- Use `JS::Handle<T>` typedefs for function parameters.
- Use `JS::MutableHandle<T>` typedefs for function out-parameters.
- Use an implicit cast from `JS::Rooted<T>` to get a JS::Handle<T>.
- Use an explicit address-of-operator on `JS::Rooted<T>` to get a JS::MutableHandle<T>.
- Return raw pointers from functions.
- Use `JS::Rooted<T>` fields when possible for aggregates, otherwise use an AutoRooter.
- Use `JS::Heap<T>` members for heap data. Note: Heap<T> are not "rooted": they must be traced!
- Do not use `JS::Rooted<T>,` JS::Handle<T> or JS::MutableHandle<T> on the heap.
- Do not use `JS::Rooted<T>` for function parameters.
- Use `JS::PersistentRooted<T>` for things that are alive until the process exits (or until you
  manually delete the `PersistentRooted` for a reason not based on GC finalization.)

### StarlingMonkey builtins API

The `builtin` API provided by StarlingMonkey is built on top of SpiderMonkey's
[`jsapi`][spidermonkey-jsapi], Mozilla's C++ interface for embedding JavaScript. It simplifies the
creation and management of native JavaScript classes and objects by abstracting common patterns and
boilerplate code required by the underlying SpiderMonkey engine. 

See also SpiderMonkey documentation on [Custom Objects][spidermonkey-objects].

This section explains how to implement a native JavaScript class using the StarlingMonkey framework.
We'll use the following JavaScript class as an example:

```js
class MyClass {
  constructor(a, b) {
    this._a = a;
    this._b = b;
  }

  get prop() {
    return 42;
  }
  method() {
    return this.a + this.b;
  }
  static get static_prop() {
    return "static";
  }
  static static_method(a, b) {
    return a + b;
  }
}
```

#### Step 1: Define the Header File (`my_class.h`)

Create a header file to declare the native class and its methods/properties.

```cpp
#ifndef BUILTINS_MY_CLASS_H
#define BUILTINS_MY_CLASS_H

#include "builtin.h"

namespace builtins {
namespace my_class {

class MyClass : public BuiltinImpl<MyClass> {
  // Instance methods
  static bool method(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool prop_get(JSContext *cx, unsigned argc, JS::Value *vp);

  // Static methods
  static bool static_method(JSContext *cx, unsigned argc, JS::Value *vp);
  static bool static_prop_get(JSContext *cx, unsigned argc, JS::Value *vp);

public:
  enum Slots { SlotA, SlotB, Count };

  static constexpr const char *class_name = "MyClass";
  static constexpr unsigned ctor_length = 2;

  static const JSFunctionSpec static_methods[];
  static const JSPropertySpec static_properties[];
  static const JSFunctionSpec methods[];
  static const JSPropertySpec properties[];

  static bool init_class(JSContext *cx, HandleObject global);
  static bool constructor(JSContext *cx, unsigned argc, Value *vp);
};

bool install(api::Engine *engine);

} // namespace my_class
} // namespace builtins

#endif // BUILTINS_MY_CLASS_H
```

#### Step 2: Implement the Class (`my_class.cpp`)

```cpp
#include "my_class.h"

namespace builtins {
namespace my_class {

const JSFunctionSpec MyClass::methods[] = {
    JS_FN("method", MyClass::method, 0, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec MyClass::properties[] = {
    JS_PSG("prop", MyClass::prop_get, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "MyClass", JSPROP_READONLY),
    JS_PS_END,
};

const JSFunctionSpec MyClass::static_methods[] = {
    JS_FN("static_method", MyClass::static_method, 2, JSPROP_ENUMERATE),
    JS_FS_END,
};

const JSPropertySpec MyClass::static_properties[] = {
    JS_PSG("static_prop", MyClass::static_prop_get, JSPROP_ENUMERATE),
    JS_PS_END,
};

// Constructor implementation
bool MyClass::constructor(JSContext *cx, unsigned argc, JS::Value *vp) {
  CTOR_HEADER("MyClass", 2);

  RootedObject self(cx, JS_NewObjectForConstructor(cx, &class_, args));
  if (!self) {
    return false;
  }

  SetReservedSlot(self, Slots::SlotA, args.get(0));
  SetReservedSlot(self, Slots::SlotB, args.get(1));

  args.rval().setObject(*self);
  return true;
}

// Instance method implementation
bool MyClass::method(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);

  double a, b;
  if (!JS::ToNumber(cx, JS::GetReservedSlot(self, Slots::SlotA), &a) ||
      !JS::ToNumber(cx, JS::GetReservedSlot(self, Slots::SlotB), &b)) {
    return false;
  }

  args.rval().setNumber(a + b);
  return true;
}

// Instance property getter implementation
bool MyClass::prop_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  METHOD_HEADER(0);
  args.rval().setInt32(42);
  return true;
}

// Static method implementation
bool MyClass::static_method(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "static_method", 2)) {
    return false;
  }

  double a, b;
  if (!JS::ToNumber(cx, args.get(0), &a) ||
      !JS::ToNumber(cx, args.get(1), &b)) {
    return false;
  }

  args.rval().setNumber(a + b);
  return true;
}

// Static property getter implementation
bool MyClass::static_prop_get(JSContext *cx, unsigned argc, JS::Value *vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setString(JS_NewStringCopyZ(cx, "static"));
  return true;
}

// Class initialization
bool MyClass::init_class(JSContext *cx, JS::HandleObject global) {
  return init_class_impl(cx, global);
}

bool install(api::Engine *engine) {
  return MyClass::init_class(engine->cx(), engine->global());
}

} // namespace my_class
} // namespace builtins
```

Deriving from `BuiltinImpl` automatically creates a `JSClass` definition using parameters provided
by the implementation:

##### Explanation of StarlingMonkey API Usage

```cpp
static constexpr JSClass class_{
    Impl::class_name,
    JSCLASS_HAS_RESERVED_SLOTS(static_cast<uint32_t>(Impl::Slots::Count)) | class_flags,
    &class_ops,
};

```

- `SetReservedSlot` and `GetReservedSlot`:

  - Used to store and retrieve internal state associated with the JavaScript object.

- `JSFunctionSpec` and `JSPropertySpec` arrays:
  - Define instance/static methods and properties exposed to JavaScript.

StarlingMonkey provides macros and helper functions to simplify native class implementation:

- `CTOR_HEADER(name, required_argc)`:

  - Initializes constructor arguments.
  - Ensures the constructor is called with the `new` keyword.
  - Checks the minimum number of arguments.

- `METHOD_HEADER(required_argc)`:

  - Initializes method arguments.
  - Ensures the receiver (~this~) is an instance of the correct class.
  - Checks the minimum number of arguments.

- `is_instance(JSObject *obj)`:

  - Used to verify if object passed is instance of builtin class.
  - Used often as assertion in class methods, for example:
    ```cpp
    JSString *MyObject::my_method(JSObject *self) {
      MOZ_ASSERT(is_instance(self));
      return JS::GetReservedSlot(self, Slots::MySlot).toString();
    }
    ```

#### Step 3: Register the Class with StarlingMonkey Engine

Finally, ensure your class is installed into the StarlingMonkey engine during initialization:

```cpp
// Example initialization code
#include "my_class.h"

void initialize_builtins(api::Engine *engine) {
  builtins::my_class::install(engine);
}

```

## Providing a custom host API implementation

The `host-apis` directory can contain implementations of the host API for different
versions of WASI—or in theory any other host interface. Those can be selected by setting the
`HOST_API` environment variable to the name of one of the directories. Currently, only an
implementation in terms of `host-apis/wasi-0.2.0` is provided, and used by default.

To provide a custom host API implementation, you can set `HOST_API` to the (absolute) path of a
directory containing that implementation.

[spidermonkey-rooting]:
  https://github.com/mozilla-spidermonkey/spidermonkey-embedding-examples/blob/next/docs/GC%20Rooting%20Guide.md
[spidermonkey-jsapi]:
  https://github.com/mozilla-spidermonkey/spidermonkey-embedding-examples/blob/next/docs/JSAPI%20Introduction.md
[spidermonkey-objects]:
  https://github.com/mozilla-spidermonkey/spidermonkey-embedding-examples/blob/next/docs/Custom%20Objects.md
