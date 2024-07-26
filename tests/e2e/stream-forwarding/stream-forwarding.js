addEventListener('fetch', async (event) => {
  try {
    if (event.request.url.endsWith('/nested')) {
      let encoder = new TextEncoder();
      let body = new TransformStream({
        start(controller) {
        },
        transform(chunk, controller) {
          controller.enqueue(encoder.encode(chunk));
        },
        flush(controller) {
        }
      });
      let writer = body.writable.getWriter();
      event.respondWith(new Response(body.readable));
      await writer.write('hello\n');
      await writer.write('world\n');
      writer.close();
      return;
    }

    let resolve;
    event.respondWith(new Promise((r) => resolve = r));
    let response = await fetch(event.request.url + 'nested');
    resolve(new Response(response.body, response));
  } catch (e) {
    console.error(e);
  }
});
