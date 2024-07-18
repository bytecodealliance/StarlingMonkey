import { serveTest } from '../test-server.js';
import { strictEqual } from '../assert.js';

export const handler = serveTest(async (t) => {
  await t.test('non-ascii-latin1-field-value', async () => {
    const response = await fetch("https://http-me.glitch.me/meow?header=cat:é");
    strictEqual(response.headers.get('cat'), "é");
  });
});
