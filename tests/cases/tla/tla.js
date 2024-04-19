let resolve;
Promise.resolve().then(() => {
  resolve();
});

await new Promise(_resolve => void (resolve = _resolve));

addEventListener('fetch', evt => evt.respondWith((async () => {
  return new Response(`hello world`, { headers: { hello: 'world' }});
})()));
