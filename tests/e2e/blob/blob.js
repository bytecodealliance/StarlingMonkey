import { strictEqual, throws } from "../../assert.js";

async function readAll(blob) {
  const reader = blob.stream().getReader();
  let totalBytes = 0;

  while (true) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    totalBytes += value.length;
  }

  strictEqual(totalBytes, blob.size, "size mismatch");
}

addEventListener("fetch", (event) =>
  event.respondWith(
    (async () => {
      try {
        const len = 10 * 1024 * 1024; // 10 MB
        let arr = new Uint8Array(len);

        arr.fill(42);
        const blob = new Blob([arr]);

        let counter = 0;
        const max = 5;

        const intervalId = setInterval(() => {
          counter++;
          console.log(`Counter: ${counter}`);

          if (counter >= max) {
            clearInterval(intervalId);
          }
        }, 1);

        await readAll(blob);

        console.log("Finished processing blob.");
        return new Response(`Large Blob`);
      } catch (e) {
        console.error(e);
      }
    })(),
  ),
);
