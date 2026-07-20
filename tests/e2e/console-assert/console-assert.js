// https://console.spec.whatwg.org/#assert
// console.assert must stay SILENT when the condition is truthy and only log
// "Assertion failed" when the condition is falsy. The condition is coerced to
// a boolean, so non-boolean values must be accepted too.
addEventListener("fetch", (event) => {
  console.log("a");
  console.assert(true); // truthy -> silent
  console.log("b");
  console.assert(false); // falsy -> logs "Assertion failed"
  console.log("c");
  console.assert(1); // truthy non-boolean -> silent
  console.log("d");
  console.assert(0); // falsy non-boolean -> logs "Assertion failed"
  console.log("e");
  event.respondWith(new Response("ok"));
});
