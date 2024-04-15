console.log('preinit');

await new Promise(resolve => setTimeout(resolve, 100));

console.log('tla completed');

addEventListener('fetch', evt => evt.respondWith((async () => {
  console.log('responding');
  return new Response(`hello world`, { headers: { hello: 'world' }});
})()));
