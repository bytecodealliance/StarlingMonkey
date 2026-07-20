import { serveTest } from "../test-server.js";
import { strictEqual, throws } from "../../assert.js";

export const handler = serveTest(async (t) => {
  await t.test("append-symbol-name-throws-and-does-not-mutate", () => {
    const p = new URLSearchParams("a=1");
    // A Symbol cannot be converted to a string: append must throw a TypeError
    // and leave the params untouched (not append a bogus empty-name entry).
    throws(() => p.append(Symbol("s"), "x"), TypeError);
    strictEqual(p.toString(), "a=1", "params unchanged after throwing append");
  });

  await t.test("append-throwing-tostring-name-propagates", () => {
    const p = new URLSearchParams();
    const bad = {
      toString() {
        throw new Error("boom");
      },
    };
    throws(() => p.append(bad, "v"), Error, "boom");
    strictEqual(p.toString(), "", "params unchanged after throwing toString");
  });

  await t.test("append-symbol-name-does-not-corrupt-attached-url", () => {
    const u = new URL("http://example.com/?a=1");
    throws(() => u.searchParams.append(Symbol("s"), "x"), TypeError);
    strictEqual(u.href, "http://example.com/?a=1", "url href unchanged");
  });

  await t.test("append-normal-still-works", () => {
    const p = new URLSearchParams("a=1");
    p.append("b", "2");
    p.append(3, 4); // non-string coercible values are fine
    strictEqual(p.toString(), "a=1&b=2&3=4", "valid appends work");
  });
});
