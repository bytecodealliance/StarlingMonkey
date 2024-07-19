import * as handlers from './handlers.js';
import { AssertionError } from '../assert.js';

export function serveTest (handler) {
  return async function handle (evt) {
    const url = new URL(evt.request.url);
    const testName = url.pathname.slice(1);
    let testFilter = url.search.slice(1);
    const filterEquals = testFilter[0] === '=';
    if (filterEquals)
      testFilter = testFilter.slice(1);

    const filter = subtest => filterEquals ? subtest === testFilter : subtest.includes(testFilter);

    let subtestCnt = 0;
    let curSubtest = null;
    try {
      await handler({
        skip (name) {
          if (!filter(name))
            return;
          console.log(`SKIPPING TEST ${testName}?${name}`);
        },
        test (name, subtest) {
          if (!filter(name))
            return;
          curSubtest = name;
          const maybePromise = subtest();
          if (maybePromise) {
            return maybePromise.then(() => {
              curSubtest = null;
              subtestCnt++;
            });
          } else {
            curSubtest = null;
            subtestCnt++;
          }
        },
        async asyncTest (name, subtest) {
          if (!filter(name))
            return;
          curSubtest = name;
          let resolve, reject;
          let promise = new Promise((res, rej) => {
            resolve = res;
            reject = rej;
          });
          subtest(resolve, reject);
          evt.waitUntil(promise);
          await promise;
          curSubtest = null;
          subtestCnt++;
        }
      });
    }
    catch (e) {
      const errorPrefix = `Error running test ${testName}${curSubtest ? '?' + curSubtest : ''}\n`;
      if (e instanceof AssertionError)
        return new Response(errorPrefix + e.message, { status: 500 });
      return new Response(errorPrefix + `Unexpected error: ${e.message}\n${e.stack}`, { status: 500 });
    }
    return new Response(`OK ${testName}${curSubtest ? '?' + curSubtest : subtestCnt > 0 || testFilter ? ` (${subtestCnt} subtests)` : ''}`);
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
