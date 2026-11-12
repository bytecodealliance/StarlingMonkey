// Features that need SpiderMonkey's Intl support (ENABLE_INTL=ON).
// Regex literals with property escapes fail at parse time without it.
addEventListener("fetch", (event) => {
  console.log(/\p{L}+/u.test("héllo"));
  console.log("a-ß_1!".replace(/[^\p{L}\p{N}]+/gu, ""));
  console.log("ﬁ".normalize("NFKD"));
  console.log("é".normalize("NFC") === "é");
  console.log(typeof Intl);
  console.log(new Intl.NumberFormat("de-DE").format(1234.5));
  event.respondWith(new Response("ok"));
});
