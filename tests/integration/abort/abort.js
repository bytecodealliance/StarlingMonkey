import { serveTest } from "../test-server.js";
import { strictEqual, throws } from "../../assert.js";

export const handler = serveTest(async (t) => {
  await t.test("throwIfAborted-throws-when-aborted", () => {
    const controller = new AbortController();
    const reason = new Error("boom");
    controller.abort(reason);

    strictEqual(controller.signal.aborted, true, "signal should be aborted");

    let thrown;
    try {
      controller.signal.throwIfAborted();
    } catch (e) {
      thrown = e;
    }

    strictEqual(thrown, reason, "throwIfAborted must throw the abort reason");
  });

  await t.test("throwIfAborted-is-noop-when-not-aborted", () => {
    const controller = new AbortController();
    strictEqual(controller.signal.aborted, false, "signal should not be aborted");
    // Must not throw when the signal is not aborted.
    let threw = false;
    try {
      controller.signal.throwIfAborted();
    } catch {
      threw = true;
    }
    strictEqual(threw, false, "throwIfAborted must not throw when not aborted");
  });

  await t.test("throwIfAborted-throws-default-AbortError", () => {
    const controller = new AbortController();
    controller.abort(); // no reason -> default AbortError DOMException

    throws(() => controller.signal.throwIfAborted());

    let name;
    try {
      controller.signal.throwIfAborted();
    } catch (e) {
      name = e.name;
    }
    strictEqual(name, "AbortError", "default abort reason is an AbortError");
  });

  await t.test("AbortSignal.abort-throwIfAborted", () => {
    const reason = new Error("static-abort");
    const signal = AbortSignal.abort(reason);
    let thrown;
    try {
      signal.throwIfAborted();
    } catch (e) {
      thrown = e;
    }
    strictEqual(thrown, reason, "AbortSignal.abort(reason).throwIfAborted() throws reason");
  });
});
