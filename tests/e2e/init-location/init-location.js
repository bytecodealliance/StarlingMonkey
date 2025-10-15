console.log(self.location.href);

addEventListener("fetch", (event) => {
  console.log(self.location.hostname);
  event.respondWith(new Response("ok"));
});
