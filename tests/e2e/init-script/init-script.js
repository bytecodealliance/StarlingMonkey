import { func } from "builtinMod";
async function handle(event) {
  console.log(func());
  return new Response(func());
}

//@ts-ignore
addEventListener('fetch', (event) => { event.respondWith(handle(event)) });
