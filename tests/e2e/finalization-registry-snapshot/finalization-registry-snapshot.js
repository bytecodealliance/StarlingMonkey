// The collection that runs before the Wizer snapshot is taken finds the target registered below
// dead. Its cleanup must have run before the snapshot, not be baked into it and run after it is
// resumed: the handler answers synchronously, before the first event loop of the resumed
// instance, so it sees the count only if the cleanup ran during pre-initialization.
let finalized = 0;
const registry = new FinalizationRegistry(() => {
  finalized += 1;
});

// In a function, so that no top-level frame still refers to the target.
function register() {
  registry.register({}, 0);
}
register();

addEventListener("fetch", (event) => event.respondWith(new Response(`finalized ${finalized}`)));
