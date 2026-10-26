// The collection that runs before the Wizer snapshot is taken finds the target registered below
// dead, and its cleanup rejects the pending top-level await. Pre-initialization must fail, as it
// does for any TLA rejection, rather than snapshot a failed module.
let reject;
const registry = new FinalizationRegistry(() => {
  reject(new Error('rejected by a FinalizationRegistry cleanup'));
});

// In a function, so that no top-level frame still refers to the target.
function register() {
  registry.register({}, 0);
}
register();

await new Promise((_, _reject) => void (reject = _reject));
