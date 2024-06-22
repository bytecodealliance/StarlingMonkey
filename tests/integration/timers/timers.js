import { serveTest } from "../test-server.js";
import { assert, strictEqual, throws, deepStrictEqual, AssertionError } from "../assert.js";

export const handler = serveTest(async (t) => {
  await t.asyncTest("clearTimeout invalid", (resolve, reject) => {
    // cleartimeout can be called with arbitrary stuff
    clearTimeout('blah');

    const dontDeleteTimeout = setTimeout(resolve, 100);

    // null converts to zero, which must not delete a real timer
    clearTimeout(null);
  });

  await t.asyncTest("setTimeout-order", (resolve, reject) => {
    let first = false;
    setTimeout(() => {
      first = true;
    }, 10);
    setTimeout(() => {
      try {
        assert(first, 'first timeout should trigger first');
      } catch (e) {
        reject(e);
        return;
      }
      resolve();
    }, 20);
  });
  await t.asyncTest("setInterval-10-times", (resolve, reject) => {
    let timeout = setTimeout(() => {
      reject(new AssertionError("Expected setInterval to be called 10 times quickly"));
    }, 1000);
    let cnt = 0;
    let interval = setInterval(() => {
      cnt++;
      if (cnt === 10) {
        clearTimeout(timeout);
        clearInterval(interval);
        resolve();
      }
    });
  });
  await t.asyncTest("setTimeout-cleared-in-callback", (resolve, reject) => {
    let id = setTimeout.call(undefined, () => {
      clearTimeout(id);
      resolve();
    }, 1);
  });
  t.test("setInterval-exposed-as-global", () => {
    strictEqual(typeof setInterval, "function", `typeof setInterval`);
  });
  t.test("setInterval-interface", () => {
    let actual = Reflect.getOwnPropertyDescriptor(globalThis, "setInterval");
    let expected = {
      writable: true,
      enumerable: true,
      configurable: true,
      value: globalThis.setInterval,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis, 'setInterval)`
    );

    strictEqual(
      typeof globalThis.setInterval,
      "function",
      `typeof globalThis.setInterval`
    );

    actual = Reflect.getOwnPropertyDescriptor(globalThis.setInterval, "length");
    expected = {
      value: 1,
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.setInterval, 'length')`
    );

    actual = Reflect.getOwnPropertyDescriptor(globalThis.setInterval, "name");
    expected = {
      value: "setInterval",
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.setInterval, 'name')`
    );
  });

  t.test("setInterval-called-as-constructor-function", () => {
    throws(
      () => {
        new setInterval();
      },
      TypeError,
      `setInterval is not a constructor`
    );
  });

  t.test("setInterval-empty-parameter", () => {
    throws(
      () => {
        setInterval();
      },
      TypeError,
      `setInterval: At least 1 argument required, but only 0 passed`
    );
  });

  t.test("setInterval-handler-parameter-not-supplied", () => {
    throws(
      () => {
        setInterval();
      },
      TypeError,
      `setInterval: At least 1 argument required, but only 0 passed`
    );
  });

  t.test("setInterval-handler-parameter-not-callable", () => {
    let non_callable_types = [
      // Primitive types
      null,
      undefined,
      true,
      1,
      1n,
      "hello",
      Symbol(),
      // After primitive types, the only remaining types are Objects and Functions
      {},
    ];
    for (const type of non_callable_types) {
      throws(
        () => {
          setInterval(type);
          // TODO: Make a TypeError
        },
        Error,
        `First argument to setInterval must be a function`
      );
    }
  });

  t.test("setInterval-timeout-parameter-not-supplied", () => {
    setInterval(function () {});
  });

  // // https://tc39.es/ecma262/#sec-tonumber
  t.test("setInterval-timeout-parameter-calls-7.1.4-ToNumber", () => {
    let sentinel;
    let requestedType;
    const test = () => {
      sentinel = Symbol();
      const timeout = {
        [Symbol.toPrimitive](type) {
          requestedType = type;
          throw sentinel;
        },
      };
      setInterval(function () {}, timeout);
    };
    throws(test);
    try {
      test();
    } catch (thrownError) {
      deepStrictEqual(thrownError, sentinel, "thrownError === sentinel");
      deepStrictEqual(requestedType, "number", 'requestedType === "number"');
    }
    throws(
      () => setInterval(function () {}, Symbol()),
      TypeError,
      `can't convert symbol to number`
    );
  });

  t.test("setInterval-timeout-parameter-negative", () => {
    setInterval(() => {}, -1);
    setInterval(() => {}, -1.1);
    setInterval(() => {}, Number.MIN_SAFE_INTEGER);
    setInterval(() => {}, Number.MIN_VALUE);
    setInterval(() => {}, -Infinity);
  });
  t.test("setInterval-timeout-parameter-positive", () => {
    setInterval(() => {}, 1);
    setInterval(() => {}, 1.1);
    setInterval(() => {}, Number.MAX_SAFE_INTEGER);
    setInterval(() => {}, Number.MAX_VALUE);
    setInterval(() => {}, Infinity);
  });
  t.test("setInterval-returns-integer", () => {
    let id = setInterval(() => {}, 1);
    deepStrictEqual(typeof id, "number", `typeof id === "number"`);
  });
  t.test("setInterval-called-unbound", () => {
    setInterval.call(undefined, () => {}, 1);
  });

  t.test("setTimeout-exposed-as-global", () => {
    deepStrictEqual(typeof setTimeout, "function", `typeof setTimeout`);
  });
  t.test("setTimeout-interface", () => {
    let actual = Reflect.getOwnPropertyDescriptor(globalThis, "setTimeout");
    let expected = {
      writable: true,
      enumerable: true,
      configurable: true,
      value: globalThis.setTimeout,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis, 'setTimeout)`
    );

    deepStrictEqual(
      typeof globalThis.setTimeout,
      "function",
      `typeof globalThis.setTimeout`
    );

    actual = Reflect.getOwnPropertyDescriptor(globalThis.setTimeout, "length");
    expected = {
      value: 1,
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.setTimeout, 'length')`
    );

    actual = Reflect.getOwnPropertyDescriptor(globalThis.setTimeout, "name");
    expected = {
      value: "setTimeout",
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.setTimeout, 'name')`
    );
  });
  t.test("setTimeout-called-as-constructor-function", () => {
    throws(
      () => {
        new setTimeout();
      },
      TypeError,
      `setTimeout is not a constructor`
    );
  });
  t.test("setTimeout-empty-parameter", () => {
    throws(
      () => {
        setTimeout();
      },
      TypeError,
      `setTimeout: At least 1 argument required, but only 0 passed`
    );
  });
  t.test("setTimeout-handler-parameter-not-supplied", () => {
    throws(
      () => {
        setTimeout();
      },
      TypeError,
      `setTimeout: At least 1 argument required, but only 0 passed`
    );
  });
  t.test("setTimeout-handler-parameter-not-callable", () => {
    let non_callable_types = [
      // Primitive types
      null,
      undefined,
      true,
      1,
      1n,
      "hello",
      Symbol(),
      // After primitive types, the only remaining types are Objects and Functions
      {},
    ];
    for (const type of non_callable_types) {
      throws(
        () => {
          setTimeout(type);
          // TODO: Make a TypeError
        },
        Error,
        `First argument to setTimeout must be a function`
      );
    }
  });
  t.test("setTimeout-timeout-parameter-not-supplied", () => {
    setTimeout(function () {});
  });
  // https://tc39.es/ecma262/#sec-tonumber
  t.test("setTimeout-timeout-parameter-calls-7.1.4-ToNumber", () => {
    let sentinel;
    let requestedType;
    const test = () => {
      sentinel = Symbol();
      const timeout = {
        [Symbol.toPrimitive](type) {
          requestedType = type;
          throw sentinel;
        },
      };
      setTimeout(function () {}, timeout);
    };
    throws(test);
    try {
      test();
    } catch (thrownError) {
      deepStrictEqual(thrownError, sentinel, "thrownError === sentinel");
      deepStrictEqual(requestedType, "number", 'requestedType === "number"');
    }
    throws(
      () => setTimeout(function () {}, Symbol()),
      TypeError,
      `can't convert symbol to number`
    );
  });

  t.test("setTimeout-timeout-parameter-negative", () => {
    setTimeout(() => {}, -1);
    setTimeout(() => {}, -1.1);
    setTimeout(() => {}, Number.MIN_SAFE_INTEGER);
    setTimeout(() => {}, Number.MIN_VALUE);
    setTimeout(() => {}, -Infinity);
  });
  t.test("setTimeout-timeout-parameter-positive", () => {
    setTimeout(() => {}, 1);
    setTimeout(() => {}, 1.1);
    setTimeout(() => {}, Number.MAX_SAFE_INTEGER);
    setTimeout(() => {}, Number.MAX_VALUE);
    setTimeout(() => {}, Infinity);
  });
  t.test("setTimeout-returns-integer", () => {
    let id = setTimeout(() => {}, 1);
    deepStrictEqual(typeof id, "number", `typeof id === "number"`);
  });
  t.test("setTimeout-called-unbound", () => {
    setTimeout.call(undefined, () => {}, 1);
  });
  t.test("setTimeout-cleared-in-callback", () => {
    let id = setTimeout.call(undefined, () => { clearTimeout(id); }, 1);
  });

  t.test("clearInterval-exposed-as-global", () => {
    deepStrictEqual(typeof clearInterval, "function", `typeof clearInterval`);
  });
  t.test("clearInterval-interface", () => {
    let actual = Reflect.getOwnPropertyDescriptor(globalThis, "clearInterval");
    let expected = {
      writable: true,
      enumerable: true,
      configurable: true,
      value: globalThis.clearInterval,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis, 'clearInterval)`
    );

    deepStrictEqual(
      typeof globalThis.clearInterval,
      "function",
      `typeof globalThis.clearInterval`
    );

    actual = Reflect.getOwnPropertyDescriptor(
      globalThis.clearInterval,
      "length"
    );
    expected = {
      value: 1,
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.clearInterval, 'length')`
    );

    actual = Reflect.getOwnPropertyDescriptor(globalThis.clearInterval, "name");
    expected = {
      value: "clearInterval",
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.clearInterval, 'name')`
    );
  });
  t.test("clearInterval-called-as-constructor-function", () => {
    throws(
      () => {
        new clearInterval();
      },
      TypeError,
      `clearInterval is not a constructor`
    );
  });
  t.test("clearInterval-id-parameter-not-supplied", () => {
    throws(
      () => {
        clearInterval();
      },
      TypeError,
      `clearInterval: At least 1 argument required, but only 0 passed`
    );
  });
  // https://tc39.es/ecma262/#sec-tonumber
  t.test("clearInterval-id-parameter-calls-7.1.4-ToNumber", () => {
    let sentinel;
    let requestedType;
    const test = () => {
      sentinel = Symbol();
      const timeout = {
        [Symbol.toPrimitive](type) {
          requestedType = type;
          throw sentinel;
        },
      };
      clearInterval(timeout);
    };
    throws(test);
    try {
      test();
    } catch (thrownError) {
      deepStrictEqual(thrownError, sentinel, "thrownError === sentinel");
      deepStrictEqual(requestedType, "number", 'requestedType === "number"');
    }
    throws(
      () => clearInterval(Symbol()),
      TypeError,
      `can't convert symbol to number`
    );
  });

  t.test("clearInterval-id-parameter-negative", () => {
    clearInterval(-1);
    clearInterval(-1.1);
    clearInterval(Number.MIN_SAFE_INTEGER);
    clearInterval(Number.MIN_VALUE);
    clearInterval(-Infinity);
  });
  t.test("clearInterval-id-parameter-positive", () => {
    clearInterval(1);
    clearInterval(1.1);
    clearInterval(Number.MAX_SAFE_INTEGER);
    clearInterval(Number.MAX_VALUE);
    clearInterval(Infinity);
  });
  t.test("clearInterval-returns-undefined", () => {
    let result = clearInterval(1);
    deepStrictEqual(typeof result, "undefined", `typeof result === "undefined"`);
  });
  t.test("clearInterval-called-unbound", () => {
    clearInterval.call(undefined, 1);
  });

  t.test("clearTimeout-exposed-as-global", () => {
    deepStrictEqual(typeof clearTimeout, "function", `typeof clearTimeout`);
  });
  t.test("clearTimeout-interface", () => {
    let actual = Reflect.getOwnPropertyDescriptor(globalThis, "clearTimeout");
    let expected = {
      writable: true,
      enumerable: true,
      configurable: true,
      value: globalThis.clearTimeout,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis, 'clearTimeout)`
    );

    deepStrictEqual(
      typeof globalThis.clearTimeout,
      "function",
      `typeof globalThis.clearTimeout`
    );

    actual = Reflect.getOwnPropertyDescriptor(
      globalThis.clearTimeout,
      "length"
    );
    expected = {
      value: 1,
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.clearTimeout, 'length')`
    );

    actual = Reflect.getOwnPropertyDescriptor(globalThis.clearTimeout, "name");
    expected = {
      value: "clearTimeout",
      writable: false,
      enumerable: false,
      configurable: true,
    };
    deepStrictEqual(
      actual,
      expected,
      `Reflect.getOwnPropertyDescriptor(globalThis.clearTimeout, 'name')`
    );
  });
  t.test("clearTimeout-called-as-constructor-function", () => {
    throws(
      () => {
        new clearTimeout();
      },
      TypeError,
      `clearTimeout is not a constructor`
    );
  });
  t.test("clearTimeout-id-parameter-not-supplied", () => {
    throws(
      () => {
        clearTimeout();
      },
      TypeError,
      `clearTimeout: At least 1 argument required, but only 0 passed`
    );
  });
  // https://tc39.es/ecma262/#sec-tonumber
  t.test("clearTimeout-id-parameter-calls-7.1.4-ToNumber", () => {
    let sentinel;
    let requestedType;
    const test = () => {
      sentinel = Symbol();
      const timeout = {
        [Symbol.toPrimitive](type) {
          requestedType = type;
          throw sentinel;
        },
      };
      clearTimeout(timeout);
    };
    throws(test);
    try {
      test();
    } catch (thrownError) {
      deepStrictEqual(thrownError, sentinel, "thrownError === sentinel");
      deepStrictEqual(requestedType, "number", 'requestedType === "number"');
    }
    throws(
      () => clearTimeout(Symbol()),
      TypeError,
      `can't convert symbol to number`
    );
  });

  t.test("clearTimeout-id-parameter-negative", () => {
    clearTimeout(-1);
    clearTimeout(-1.1);
    clearTimeout(Number.MIN_SAFE_INTEGER);
    clearTimeout(Number.MIN_VALUE);
    clearTimeout(-Infinity);
  });
  t.test("clearTimeout-id-parameter-positive", () => {
    clearTimeout(1);
    clearTimeout(1.1);
    clearTimeout(Number.MAX_SAFE_INTEGER);
    clearTimeout(Number.MAX_VALUE);
    clearTimeout(Infinity);
  });
  t.test("clearTimeout-returns-undefined", () => {
    let result = clearTimeout(1);
    deepStrictEqual(typeof result, "undefined", `typeof result === "undefined"`);
  });
  t.test("clearTimeout-called-unbound", () => {
    clearTimeout.call(undefined, 1);
  });
});
