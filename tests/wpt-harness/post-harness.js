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
  globalThis.baseURL = new URL(input);
  globalThis.location = baseURL;
  try {
    let response = await fetch(input);
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
    eval(wpt_test_script);
  }
}

async function loadMetaScript(path) {
  let response = await fetch(path);
  let metaSource = await response.text();
  // Somewhat annoyingly, the WPT harness includes META scripts as <script src=[path]> tags,
  // which don't create their own scope for `const` and `let bindings, as `eval` does.
  // That means that running them in `eval` doesn't make any `const` and `let` bound values
  // available to code outside of the current `eval`, which breaks various WPT files.
  //
  // Short of introducing a host call for evaluating code in the same way a `<script>` tag does
  // the only way to get around that is to replace all these bindings with `var`. Which is
  // an ugly hack, but works reasonably well.
  let lines = metaSource.split("\n");
  lines = lines.map(line => {
    if (line.indexOf("const ") == 0) {
      return "var " + line.substr(6);
    }
    if (line.indexOf("let ") == 0) {
      return "var " + line.substr(4);
    }
    return line;
  });
  metaSource = lines.join("\n");
  metaSource += "\n//# sourceURL=" + path;
  return metaSource;
}

addEventListener("fetch", event => event.respondWith(handleRequest(event)));
