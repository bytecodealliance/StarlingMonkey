
function handle(_request: Request): Response {
  let thingy = 'world';
  let message = `Hello, ${thingy}`;
  return new Response(message, {
    status: 200,
    headers: {
      'content-type': 'text/plain'
    }
  })
}

//@ts-ignore
addEventListener('fetch', (event: FetchEvent) => {
  event.respondWith(handle(event.request));
});
