addEventListener('fetch', async (event) => {
  event.respondWith(handleRequest(event));
});

async function handleRequest(event) {
  if (event.request.url.includes('/api/upstream')) {
    console.log(`[forwarded]: ${await event.request.text()}`)
    return new Response('Successfully forwarded\n');
  }

  let controller;
  let stream = new ReadableStream({
    start(c) {
      controller = c;
    },
  });
  const [forwardStream, logStream] = stream.tee();

  const forwardRequest = new Request('/api/upstream', { body: forwardStream, method: "POST" });

  let pending = fetch(forwardRequest);
  await controller.enqueue(new TextEncoder().encode("Hello, "));
  await controller.enqueue(new TextEncoder().encode("World!"));
  await controller.close();
  const response = await pending;
  await logRequestBody(logStream);
  return response;
}

async function logRequestBody(stream) {
  let text = "";
  let decoder = new TextDecoder();
  const reader = stream.getReader();
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    text += decoder.decode(value, { stream: true });
  }
  text += decoder.decode(new Uint8Array(), { stream: false });
  console.log(`[tee]: ${text}`);
}
