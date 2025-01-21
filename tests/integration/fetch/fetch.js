import { serveTest } from '../test-server.js';
import { strictEqual, deepStrictEqual, throws } from '../../assert.js';

export const handler = serveTest(async (t) => {
  await t.test('headers-non-ascii-latin1-field-value', async () => {
    const response = await fetch("https://http-me.glitch.me/meow?header=cat:é");
    strictEqual(response.headers.get('cat'), "é");
  });

  t.test('request-clone-bad-calls', () => {
    throws(() => new Request.prototype.clone(), TypeError);
    throws(() => new Request.prototype.clone.call(undefined), TypeError);
  });

  await t.test('request-clone-valid', async () => {
    {
      const request = new Request('https://www.fastly.com', {
        headers: {
            hello: 'world'
        },
        body: 'te',
        method: 'post'
      });
      const newRequest = request.clone();
      strictEqual(newRequest instanceof Request, true, 'newRequest instanceof Request');
      strictEqual(newRequest.method, request.method, 'newRequest.method');
      strictEqual(newRequest.url, request.url, 'newRequest.url');
      deepStrictEqual([...newRequest.headers], [...request.headers], 'newRequest.headers');
      strictEqual(request.bodyUsed, false, 'request.bodyUsed');
      strictEqual(newRequest.bodyUsed, false, 'newRequest.bodyUsed');
      strictEqual(newRequest.body instanceof ReadableStream, true, 'newRequest.body instanceof ReadableStream');
    }

    {
      const request = new Request('https://www.fastly.com', {
        method: 'get'
      })
      const newRequest = request.clone();

      strictEqual(newRequest.bodyUsed, false, 'newRequest.bodyUsed');
      strictEqual(newRequest.body, null, 'newRequest.body');
    }
  });

  await t.test('request-clone-invalid', async () => {
    const request = new Request('https://www.fastly.com', {
      headers: {
          hello: 'world'
      },
      body: 'te',
      method: 'post'
    });
    await request.text();
    throws(() => request.clone());
  });

  await t.test('blob-partial-fetch', async () => {
    const data = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const blobUrl = URL.createObjectURL(blob);

    // Perform a ranged fetch (requesting the first 5 bytes: 0-4)
    const response = await fetch(blobUrl, {
      headers: {
        'Range': 'bytes=0-4'
      }
    });

    // Verify status and status text for partial content
    strictEqual(response.status, 206, 'response.status should be 206 for partial content');
    strictEqual(response.statusText, 'Partial Content', 'response.statusText should be "Partial Content"');

    // Check headers
    const contentType = response.headers.get('Content-Type');
    const contentLength = response.headers.get('Content-Length');
    const contentRange = response.headers.get('Content-Range');

    strictEqual(contentType, 'application/octet-stream', 'Content-Type matches blob type');
    strictEqual(contentLength, '5', 'Content-Length matches the length of requested range (5 bytes)');
    strictEqual(contentRange, 'bytes 0-4/10', 'Content-Range matches requested bytes and total length');

    // Verify returned data
    const buf = new Uint8Array(await response.arrayBuffer());
    deepStrictEqual(buf, new Uint8Array([0,1,2,3,4]), 'returned data should match the requested byte range');

    URL.revokeObjectURL(blobUrl);
  });

  await t.test('blob-partial-fetch-range-open-end', async () => {
    const data = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const blobUrl = URL.createObjectURL(blob);

    const response = await fetch(blobUrl, {
      headers: { 'Range': 'bytes=5-' }
    });

    strictEqual(response.status, 206, 'Should return partial content for open-ended range');
    strictEqual(response.statusText, 'Partial Content', 'Should return "Partial Content"');

    const lengthRemaining = data.length - 5; // 5 bytes (indices 5 to 9)
    strictEqual(response.headers.get('Content-Length'), String(lengthRemaining), 'Content-Length should match the remaining bytes');
    strictEqual(response.headers.get('Content-Range'), 'bytes 5-9/10', 'Content-Range should reflect the requested segment (5 through end)');

    const buf = new Uint8Array(await response.arrayBuffer());
    deepStrictEqual(buf, data.slice(5), 'Returned data should match bytes 5 through the end');

    URL.revokeObjectURL(blobUrl);
  });

  await t.test('blob-partial-fetch-range-open-start', async () => {
    const data = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const blobUrl = URL.createObjectURL(blob);

    const response = await fetch(blobUrl, {
      headers: { 'Range': 'bytes=-5' }
    });

    strictEqual(response.status, 206, 'Should return 206');
    strictEqual(response.statusText, 'Partial Content', 'Should return "Partial Content"');

    const lastFive = data.slice(data.length - 5, data.length);
    strictEqual(response.headers.get('Content-Length'), '5', 'Content-Length should be last 5 bytes');
    strictEqual(response.headers.get('Content-Range'), 'bytes 5-9/10', 'Content-Range matches the last 5 bytes');

    const buf = new Uint8Array(await response.arrayBuffer());
    deepStrictEqual(buf, lastFive, 'Returned data should match the last 5 bytes of the resource');

    URL.revokeObjectURL(blobUrl);
  });

  await t.test('file-fetch', async () => {
    const data = new Uint8Array([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const file = new File([blob], "dummy.txt");

    const fileUrl = URL.createObjectURL(file);
    const response = await fetch(fileUrl);

    strictEqual(response.status, 200, 'Should return 200');
    strictEqual(response.statusText, 'OK', 'Should return "OK"');

    const buf = new Uint8Array(await response.arrayBuffer());
    deepStrictEqual(buf, data, 'Returned data should match data');

    URL.revokeObjectURL(fileUrl);
  });
});
