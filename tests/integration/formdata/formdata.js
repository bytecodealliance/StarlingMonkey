import { serveTest } from "../test-server.js";
import { strictEqual } from "../../assert.js";

function multipart(parts, boundary = "X") {
  let body = "";
  for (const p of parts) {
    body += `--${boundary}\r\n`;
    body += `Content-Disposition: form-data; name="${p.name}"\r\n\r\n`;
    body += `${p.value}\r\n`;
  }
  body += `--${boundary}--\r\n`;
  return new Response(new TextEncoder().encode(body), {
    headers: { "content-type": `multipart/form-data; boundary=${boundary}` },
  });
}

export const handler = serveTest(async (t) => {
  await t.test("formData-preserves-leading-bom", async () => {
    // Per the Fetch spec a part's value is "UTF-8 decode without BOM", which
    // preserves a leading U+FEFF rather than stripping it.
    const fd = await multipart([{ name: "field", value: "﻿hello" }]).formData();
    const value = fd.get("field");
    strictEqual(value.length, 6, "leading BOM must be preserved");
    strictEqual(value.charCodeAt(0), 0xfeff, "first code unit is U+FEFF");
    strictEqual(value, "﻿hello", "value keeps the BOM");
  });

  await t.test("formData-non-bom-content-unchanged", async () => {
    const fd = await multipart([{ name: "a", value: "plain" }]).formData();
    strictEqual(fd.get("a"), "plain", "ordinary content decodes unchanged");
  });

  await t.test("formData-bom-in-the-middle-unchanged", async () => {
    // A BOM that is not at the very start is always preserved.
    const fd = await multipart([{ name: "m", value: "ab﻿cd" }]).formData();
    strictEqual(fd.get("m"), "ab﻿cd", "interior BOM is preserved");
  });
});
