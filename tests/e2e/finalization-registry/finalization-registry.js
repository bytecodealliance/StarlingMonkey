// A FinalizationRegistry callback runs once a collection has found its target dead. The
// collection is started by allocation, and the callback runs in the event loop, after the
// microtask checkpoint that follows it.
let finalized = 0;
const registry = new FinalizationRegistry(() => {
  finalized += 1;
});

async function handle() {
  registry.register({}, 0);

  // Allocate until a collection has run and its cleanup has been called, giving up after a
  // bounded number of attempts.
  for (let i = 0; i < 1000 && finalized === 0; i++) {
    const buffer = new Uint8Array(2 << 20);
    buffer[0] = 1;
    // Wait for a task, so that the cleanups queued by a collection can run.
    await new Promise((resolve) => setTimeout(resolve, 0));
  }

  return new Response(finalized > 0 ? "finalized" : "not finalized");
}

addEventListener("fetch", (event) => event.respondWith(handle()));
