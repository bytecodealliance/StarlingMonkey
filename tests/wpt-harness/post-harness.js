/* eslint-env serviceworker */
/* global add_completion_callback setup done */

let completionPromise = new Promise((resolve) => {
    add_completion_callback(function(tests, harness_status, asserts) {
      resolve({tests, harness_status, asserts});
    });
});

setup({ explicit_done: true });

async function handleRequest(event) {
  let url = new URL(event.request.url);
  console.log(`running test ${url.pathname}`);
  let input = `http://web-platform.test:8000${url.pathname}${url.search}`;
  globalThis.wpt_baseURL = new URL(input);
  globalThis.location = wpt_baseURL;
  try {
    let response = await fetch(input);
    if (!response.ok) {
      throw new Error(`Failed to fetch ${input}: ${response.status} ${response.statusText}`);
    }

    let testSource = await response.text();
    testSource += "\n//# sourceURL=" + url.pathname;

    let scripts = [];

    // eslint-disable-next-line no-unused-vars
    for (let [_, path] of testSource.matchAll(/META: *script=(.+)/g)) {
      let metaSource = await loadMetaScript(path, input);
      scripts.push(metaSource);
    }

    scripts.push(testSource);
    evalAllScripts(scripts);
    done();

    let {tests} = await completionPromise;

    return new Response(JSON.stringify(tests, null, 2), { headers: { "content-encoding" : "application/json" } });
  } catch (e) {
    console.log(`error: ${e}, stack:\n${e.stack}`);
    return new Response(`{
      "error": {
        "message": ${JSON.stringify(e.message)},
        "stack": ${JSON.stringify(e.stack)}
      }
    }`, { status: 500 });
  }
}

function evalAllScripts(wpt_test_scripts) {
  for (let wpt_test_script of wpt_test_scripts) {
    evalScript(wpt_test_script);
  }
}

async function loadMetaScript(path) {
  let response = await fetch(path);
  let metaSource = await response.text();
  metaSource += "\n//# sourceURL=" + path;
  return metaSource;
}

addEventListener("fetch", event => event.respondWith(handleRequest(event)));
