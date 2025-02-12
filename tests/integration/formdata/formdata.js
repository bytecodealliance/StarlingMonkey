import { serveTest } from "../test-server.js";
import { assert, strictEqual } from "../../assert.js";

export const handler = serveTest(async (t) => {
  await t.test('form-data-encode', async () => {
    async function readStream(stream) {
      const reader = stream.getReader();
      const chunks = [];
      let totalLen = 0;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
        totalLen += value.length;
      }
      const joined = new Uint8Array(totalLen);
      let offset = 0;
      for (const chunk of chunks) {
        joined.set(chunk, offset);
        offset += chunk.length;
      }
      return joined.buffer;
    }

    const form = new FormData();
    form.append('field1', 'value1');
    form.append('field2', 'value2');

    const file = new File(['Hello World!'], 'dummy.txt', { type: 'foo' });
    form.append('file1', file);

    const req = new Request('https://example.com', { method: 'POST', body: form });

    const contentType = req.headers.get('Content-Type') || '';
    assert(
      contentType.startsWith('multipart/form-data; boundary='),
      `Content-Type should be multipart/form-data; got: ${contentType}`
    );

    const boundary = contentType.split('boundary=')[1];
    assert(boundary, 'Boundary must be present in the Content-Type');

    const arrayBuf = await readStream(req.body);
    const bodyStr = new TextDecoder().decode(arrayBuf);
    const lines = bodyStr.split('\r\n');

    const expectedLines = [
      `--${boundary}`,
      'Content-Disposition: form-data; name="field1"',
      '',
      'value1',
      `--${boundary}`,
      'Content-Disposition: form-data; name="field2"',
      '',
      'value2',
      `--${boundary}`,
      'Content-Disposition: form-data; name="file1"; filename="dummy.txt"',
      'Content-Type: foo',
      '',
      'Hello World!',
      `--${boundary}--`,
    ];

    strictEqual(
      lines.length,
      expectedLines.length,
      `Expected ${expectedLines.length} lines, got ${lines.length}`
    );

    for (let i = 0; i < expectedLines.length; i++) {
      strictEqual(
        lines[i],
        expectedLines[i],
        `Mismatch at line ${i}. Actual: '${lines[i]}'  Expected: '${expectedLines[i]}'`
      );
    }
  });
});
