function interpreted(value) {
  return eval("value + 1");
}

addEventListener("fetch", event => {
  event.respondWith(new Response(`fallback ${interpreted(41)}\n`));
});
