import * as handlers from './handlers.js';
import { AssertionError } from './assert.js';

export function serveTest (handler) {
  return async function handle (evt) {
    const testName = new URL(evt.request.url).pathname.slice(1);
    try {
      await handler();
    }
    catch (e) {
      if (e instanceof AssertionError)
        return new Response(e.message, { status: 500 });
      return new Response(`Unexpected error: ${e.message}\n${e.stack}`, { status: 500 });
    }
    return new Response(`${testName} OK`);
  };
}

addEventListener('fetch', evt => {
  let testName;
  try {
    testName = new URL(evt.request.url).pathname.slice(1);
  }
  catch (e) {
    return void evt.respondWith(new Response(`Unable to parse test URL ${evt.request.url}, ${e.message}\n${e.stack}`, { status: 500 }));
  }

  const testHandler = handlers[testName];
  if (!testHandler) {
    return void evt.respondWith(new Response(`Unable to find test handler ${testName}`, { status: 500 }));
  }

  try {
    const testHandlerPromise = testHandler(evt);
    evt.respondWith(testHandlerPromise);
  }
  catch (e) {
    return void evt.respondWith(new Response(`Unable to run test handler ${testName}, ${e.message}\n${e.stack}`, { status: 500 }));
  }
});
