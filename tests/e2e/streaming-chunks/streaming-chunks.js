addEventListener('fetch', evt => {
  evt.respondWith((async () => {
    return new Response(new ReadableStream({
      start(controller) {
        controller.enqueue(new TextEncoder().encode('hello world'));
        controller.close();
      },
    }));
  }));
});
