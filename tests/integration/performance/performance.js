import { serveTest } from "../test-server.js";
import { deepStrictEqual, strictEqual, throws } from "../../assert.js";

export const handler = serveTest(async (t) => {
  t.test("Performance-interface", () => {
    {
      let actual = Reflect.ownKeys(Performance);
      let expected = ["prototype", "length", "name"];
      deepStrictEqual(actual, expected, `Reflect.ownKeys(Performance)`);
    }

    // Check the prototype descriptors are correct
    {
      let actual = Reflect.getOwnPropertyDescriptor(Performance, "prototype");
      let expected = {
        value: Performance.prototype,
        writable: false,
        enumerable: false,
        configurable: false,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance, 'prototype')`
      );
    }

    // Check the constructor function's defined parameter length is correct
    {
      let actual = Reflect.getOwnPropertyDescriptor(Performance, "length");
      let expected = {
        value: 0,
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance, 'length')`
      );
    }

    // Check the constructor function's name is correct
    {
      let actual = Reflect.getOwnPropertyDescriptor(Performance, "name");
      let expected = {
        value: "Performance",
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance, 'name')`
      );
    }

    // Check the prototype has the correct keys
    {
      let actual = Reflect.ownKeys(Performance.prototype);
      let expected = ["constructor", "timeOrigin", "now", Symbol.toStringTag];
      deepStrictEqual(
        actual,
        expected,
        `Reflect.ownKeys(Performance.prototype)`
      );
    }

    // Check the constructor on the prototype is correct
    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype,
        "constructor"
      );
      let expected = {
        writable: true,
        enumerable: false,
        configurable: true,
        value: Performance.prototype.constructor,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance.prototype, 'constructor')`
      );

      strictEqual(
        typeof Performance.prototype.constructor,
        "function",
        `typeof Performance.prototype.constructor`
      );
    }

    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype.constructor,
        "length"
      );
      let expected = {
        value: 0,
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance.prototype.constructor, 'length')`
      );
    }

    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype.constructor,
        "name"
      );
      let expected = {
        value: "Performance",
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance.prototype.constructor, 'name')`
      );
    }

    // Check the Symbol.toStringTag on the prototype is correct
    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype,
        Symbol.toStringTag
      );
      let expected = {
        value: "performance",
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance.prototype, [Symbol.toStringTag])`
      );

      strictEqual(
        typeof Performance.prototype[Symbol.toStringTag],
        "string",
        `typeof Performance.prototype[Symbol.toStringTag]`
      );
    }

    // Check the timeOrigin property is correct
    {
      let descriptors = Reflect.getOwnPropertyDescriptor(
        Performance.prototype,
        "timeOrigin"
      );
      strictEqual(
        descriptors.enumerable,
        true,
        `Reflect.getOwnPropertyDescriptor(Performance, 'timeOrigin').enumerable`
      );
      strictEqual(
        descriptors.configurable,
        true,
        `Reflect.getOwnPropertyDescriptor(Performance, 'timeOrigin').configurable`
      );
      strictEqual(
        descriptors.value,
        undefined,
        `Reflect.getOwnPropertyDescriptor(Performance, 'timeOrigin').value`
      );
      strictEqual(
        descriptors.set,
        undefined,
        `Reflect.getOwnPropertyDescriptor(Performance, 'timeOrigin').set`
      );
      strictEqual(
        typeof descriptors.get,
        "function",
        `typeof Reflect.getOwnPropertyDescriptor(Performance, 'timeOrigin').get`
      );

      strictEqual(
        typeof Performance.prototype.timeOrigin,
        "number",
        `typeof Performance.prototype.timeOrigin`
      );
    }

    // Check the now property is correct
    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype,
        "now"
      );
      let expected = {
        writable: true,
        enumerable: true,
        configurable: true,
        value: Performance.prototype.now,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance, 'now')`
      );

      strictEqual(
        typeof Performance.prototype.now,
        "function",
        `typeof Performance.prototype.now`
      );
    }

    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype.now,
        "length"
      );
      let expected = {
        value: 0,
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance.prototype.now, 'length')`
      );
    }

    {
      let actual = Reflect.getOwnPropertyDescriptor(
        Performance.prototype.now,
        "name"
      );
      let expected = {
        value: "now",
        writable: false,
        enumerable: false,
        configurable: true,
      };
      deepStrictEqual(
        actual,
        expected,
        `Reflect.getOwnPropertyDescriptor(Performance.prototype.now, 'name')`
      );
    }
  });

  t.test("globalThis.performance", () => {
    strictEqual(
      globalThis.performance instanceof Performance,
      true,
      `globalThis.performance instanceof Performance`
    );
  });

  t.test("globalThis.performance.now", () => {
    throws(() => new performance.now());
    strictEqual(typeof performance.now(), "number");
    strictEqual(performance.now() > 0, true);
    strictEqual(Number.isNaN(performance.now()), false);
    strictEqual(Number.isFinite(performance.now()), true);
    strictEqual(performance.now() < Date.now(), true);
  });

  t.test("globalThis.performance.timeOrigin", () => {
    strictEqual(typeof performance.timeOrigin, "number");
    strictEqual(performance.timeOrigin > 0, true);
    strictEqual(Number.isNaN(performance.timeOrigin), false);
    strictEqual(Number.isFinite(performance.timeOrigin), true);
    strictEqual(performance.timeOrigin < Date.now(), true);
  });
});
