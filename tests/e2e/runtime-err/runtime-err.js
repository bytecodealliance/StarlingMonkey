import { strictEqual, deepStrictEqual, throws } from "../../assert.js";

addEventListener("fetch", (evt) =>
  evt.respondWith(
    (async () => {
      throw new Error('runtime error', { cause: new Error('error cause') });
    })()
  )
);
