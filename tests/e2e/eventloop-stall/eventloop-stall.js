import { strictEqual, deepStrictEqual, throws } from "../../assert.js";

addEventListener("fetch", (evt) =>
  evt.respondWith(
    (async () => {
      return new Response(new ReadableStream({
        start(_controller) {
          // stall
        },
      }));
    })()
  )
);
