import { serveTest } from "../test-server.js";
import { strictEqual, throws } from "../../assert.js";

const AB = new Uint8Array([0x61, 0x62]); // "ab"

export const handler = serveTest(async (t) => {
  await t.test("decode-options-null-uses-defaults", () => {
    // `options` is a WebIDL dictionary: null means defaults, not a type error.
    strictEqual(new TextDecoder().decode(AB, null), "ab", "null options decode");
  });

  await t.test("decode-options-undefined-and-omitted", () => {
    strictEqual(new TextDecoder().decode(AB, undefined), "ab", "undefined options");
    strictEqual(new TextDecoder().decode(AB), "ab", "omitted options");
  });

  await t.test("decode-options-object", () => {
    strictEqual(new TextDecoder().decode(AB, { stream: false }), "ab", "object options");
  });

  await t.test("decode-options-non-object-still-throws", () => {
    // A defined, non-null, non-object value is still a TypeError.
    throws(() => new TextDecoder().decode(AB, "nope"), TypeError);
    throws(() => new TextDecoder().decode(AB, 42), TypeError);
  });

  await t.test("constructor-options-null-uses-defaults", () => {
    strictEqual(new TextDecoder("utf-8", null).decode(AB), "ab", "ctor null options");
  });
});
