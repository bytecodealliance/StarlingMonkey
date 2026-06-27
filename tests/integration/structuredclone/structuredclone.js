import { serveTest } from "../test-server.js";
import { strictEqual } from "../../assert.js";

export const handler = serveTest(async (t) => {
  await t.test("structuredClone-blob-preserves-type", () => {
    const b = new Blob(["hello"], { type: "text/plain" });
    const c = structuredClone(b);
    strictEqual(c.size, 5, "size is preserved");
    strictEqual(c.type, "text/plain", "type is preserved");
  });

  await t.test("structuredClone-blob-empty-type", () => {
    const b = new Blob(["x"]);
    const c = structuredClone(b);
    strictEqual(c.size, 1, "size is preserved");
    strictEqual(c.type, "", "empty type is preserved");
  });

  await t.test("structuredClone-blob-content-and-type", async () => {
    const b = new Blob(["hello world"], { type: "text/markdown" });
    const c = structuredClone(b);
    strictEqual(await c.text(), "hello world", "content is preserved");
    strictEqual(c.type, "text/markdown", "type is preserved");
  });
});
