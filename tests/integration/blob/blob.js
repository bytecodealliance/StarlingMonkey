import { serveTest } from "../test-server.js";
import { strictEqual, deepStrictEqual } from "../../assert.js";

async function readStream(stream) {
  const reader = stream.getReader();
  const chunks = [];

  while (true) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    chunks.push(value);
  }

  let totalLen = 0;
  for (const chunk of chunks) {
    totalLen += chunk.length;
  }

  const result = new Uint8Array(totalLen);

  let offset = 0;
  for (const chunk of chunks) {
    result.set(chunk, offset);
    offset += chunk.length;
  }

  return result;
}

export const handler = serveTest(async (t) => {
  await t.test("blob-multiple-streams", async () => {
    // Native implementation uses 8192 chunks, make sure we use few of those.
    const size = 3 * 8192;
    const buffer = new Uint8Array(size);
    for (let i = 0; i < size; i++) {
      buffer[i] = i % 256;
    }

    const blob = new Blob([buffer]);
    const stream1 = blob.stream();
    const stream2 = blob.stream();

    // Read streams concurrently
    const [data1, data2] = await Promise.all([
      readStream(stream1),
      readStream(stream2),
    ]);

    strictEqual(data1.length, size, "size matches");
    strictEqual(data2.length, size, "size matches");
    deepStrictEqual(data1, buffer, "buffer content matches");
    deepStrictEqual(data2, buffer, "buffer content matches");
  });
});
