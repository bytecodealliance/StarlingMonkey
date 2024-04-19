let resolve;
Promise.resolve().then(() => {
  resolve();
});

await new Promise(_resolve => void (resolve = _resolve));

let runtimePromiseResolve;
let runtimePromise = new Promise(resolve => runtimePromiseResolve = resolve);

addEventListener('fetch', evt => evt.respondWith((async () => {
  runtimePromiseResolve();
  return new Response(`hello world`, { headers: { hello: 'world' }});
})()));

await runtimePromise;

console.log('YO');
