import { strictEqual, deepStrictEqual, throws } from "../../assert.js";

addEventListener("fetch", (evt) =>
  evt.respondWith(
    (async () => {
      strictEqual(evt.request.headers.get("EXAMPLE-HEADER"), "Header Value");
      throws(
        () => {
          evt.request.headers.delete("user-agent");
        },
        TypeError,
        "Headers.delete: Headers are immutable"
      );
      const response = new Response("test", {
        headers: [...evt.request.headers.entries()].filter(
          ([name]) => name !== "content-type" && name !== 'content-length'
        ),
      });

      response.headers.append("Set-cookie", "A");
      response.headers.append("Another", "A");
      response.headers.append("set-cookie", "B");
      response.headers.append("another", "B");

      deepStrictEqual(
        [...response.headers],
        [
          ["accept", "*/*"],
          ["another", "A, B"],
          ["content-length", "4"],
          ["content-type", "text/plain;charset=UTF-8"],
          ["example-header", "Header Value"],
          ["set-cookie", "A"],
          ["set-cookie", "B"],
          ["user-agent", "test-agent"],
        ]
      );
      response.headers.delete("accept");
      return response;
    })()
  )
);
