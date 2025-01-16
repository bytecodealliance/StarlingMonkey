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

      request.headers.set("foo", "bar")
      const newRequest = request.clone();

      strictEqual(newRequest instanceof Request, true, 'newRequest instanceof Request');
      strictEqual(newRequest.method, request.method, 'newRequest.method');
      strictEqual(newRequest.url, request.url, 'newRequest.url');
      deepStrictEqual([...newRequest.headers], [...request.headers], 'newRequest.headers');
      strictEqual(request.bodyUsed, false, 'request.bodyUsed');
      strictEqual(newRequest.bodyUsed, false, 'newRequest.bodyUsed');
      strictEqual(newRequest.body instanceof ReadableStream, true, 'newRequest.body instanceof ReadableStream');

      strictEqual(newRequest.headers.get("foo"), "bar", 'newRequest.status pre-modification');
      request.headers.set("foo", "bao")
      strictEqual(newRequest.headers.get("foo"), "bar", 'newRequest.status post-modification');
      strictEqual(request.headers.get("foo"), "bao", 'request.status post-modification');
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

  t.test('response-clone-bad-calls', () => {
    throws(() => new Response.prototype.clone(), TypeError);
    throws(() => new Response.prototype.clone.call(undefined), TypeError);
  });

  await t.test('response-clone-valid', async () => {
    {
      const response = new Response('test body', {
        headers: {
          hello: 'world'
        },
        status: 200,
        statusText: 'Success'
      });
      response.headers.set("foo", "bar")
      const newResponse = response.clone();

      strictEqual(newResponse instanceof Response, true, 'newResponse instanceof Request');
      strictEqual(response.bodyUsed, false, 'response.bodyUsed');
      strictEqual(newResponse.bodyUsed, false, 'newResponse.bodyUsed');
      deepStrictEqual([...newResponse.headers], [...response.headers], 'newResponse.headers');
      strictEqual(newResponse.status, 200, 'newResponse.status');
      strictEqual(newResponse.statusText, 'Success', 'newResponse.statusText');
      strictEqual(newResponse.body instanceof ReadableStream, true, 'newResponse.body instanceof ReadableStream');

      strictEqual(newResponse.headers.get("foo"), "bar", 'newResponse.status pre-modification');
      response.headers.set("foo", "bao")
      strictEqual(newResponse.headers.get("foo"), "bar", 'newResponse.status post-modification');
      strictEqual(response.headers.get("foo"), "bao", 'response.status post-modification');
    }

    {
      const response = new Response(null, {
        status: 404,
        statusText: "Not found",
      });
      const newResponse = response.clone();
      strictEqual(newResponse.bodyUsed, false, 'newResponse.bodyUsed');
      strictEqual(newResponse.body, null, 'newResponse.body');
    }
  });

  await t.test('response-clone-invalid', async () => {
    const response = new Response('test body', {
      status: 200,
      statusText: "Success"
    });
    await response.text();
    throws(() => response.clone());
  });
});
