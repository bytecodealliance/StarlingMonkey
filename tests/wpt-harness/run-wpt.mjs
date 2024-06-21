import { execFile } from "child_process";
import { on, once } from "events";
import { existsSync, mkdirSync, rmSync, readFileSync, writeFileSync } from "fs";
import http from "http";
import path from "path";

let LogLevel = {
  Quiet: 0,
  Verbose: 1,
  VeryVerbose: 2,
};

function relativePath(path) {
  return new URL(path, import.meta.url).pathname;
}

const SKIP_PREFIX = "SKIP ";
const SLOW_PREFIX = "SLOW ";

const config = {
  wasmtime: {
    external: false,
    addr: "127.0.0.1:7676",
    runtime: "wpt-runtime.wasm",
  },
  wptServer: {
    external: false,
    root: relativePath("./wpt"),
  },
  server: {
    port: 7879,
  },
  tests: {
    list: relativePath("tests.json"),
    expectations: relativePath("expectations"),
    updateExpectations: false,
    pattern: "",
  },
  results: {
    pageTemplate: relativePath("results-page.template.html"),
    sectionTemplate: relativePath("results-section.template.html"),
    sectionErrorTemplate: relativePath("results-section-error.template.html"),
  },
  interactive: false,
  skipSlowTests: false,
  logLevel: LogLevel.Quiet,
};

const ArgParsers = {
  "--runtime": {
    help: `Path to .wasm file containing the WPT tests runtime to use (default: ${config.wasmtime.runtime})`,
    cmd: val => { config.wasmtime.runtime = val }
  },
  "--port": {
    help: `Port to run the server on in interactive mode (default: ${config.server.port})`,
    cmd: val => {
      config.server.port = parseInt(val, 10);
      if (isNaN(config.server.port)) {
        return `Invalid value for --port: ${val}`;
      }
    }
  },
  "--external-wasmtime": {
    help: "Don't start a Wasmtime instance internally (default: false)",
    cmd: () => { config.wasmtime.external = true; }
  },
  "--wpt-root": {
    help: "Root directory of the WPT suite (default: ./wpt)",
    cmd: val => { config.wptServer.root = val; }
  },
  "--external-wpt-server": {
    help: "Don't start an instance of the WPT server internally (default: false)",
    cmd: () => { config.wptServer.external = true; }
  },
  "--expectations": {
    help: `Path to the directory containing test expectations files (default: ${config.tests.expectations}}`,
    cmd: val => { config.tests.expectations = val; }
  },
  "--update-expectations": {
    help: "Update test expectations file with results from the current run (default: false)",
    cmd: () => { config.tests.updateExpectations = true; }
  },
  "--interactive": {
    help: "Start a server instead of directly running tests",
    cmd: () => { config.interactive = true; }
  },
  "--skip-slow-tests": {
    help: "Skip tests that take a long time, in particular in debug builds of the runtime",
    cmd: () => { config.skipSlowTests = true; }
  },
  "-v": {
    help: "Verbose output",
    cmd: () => { config.logLevel = LogLevel.Verbose; }
  },
  "-vv": {
    help: "Very verbose output",
    cmd: () => { config.logLevel = LogLevel.VeryVerbose; }
  },
  "--help": {
    help: "Print this help message",
    cmd: () => {
      console.log(`Usage:
    node run-wpt.mjs [...options] [pattern]

If a pattern is provided, only tests whose path contains the pattern will be run

Options:`);

      for (let [name, parser] of Object.entries(ArgParsers)) {
        console.log(`    ${(name + (parser.cmd.length > 0 ? "=value" : "")).padEnd(25)}${parser.help}`);
      }
      process.exit(0);
     }

  },
}

function applyConfig(argv) {
  for (let entry of argv.slice(2)) {
    if (entry[0] != "-") {
      config.tests.pattern = entry;
      continue;
    }
    let [arg, val] = entry.split("=");
    let result = undefined;
    let parser = ArgParsers[arg];
    if (parser) {
      result = parser.cmd(val);
    } else {
      result = `Unknown argument: ${arg}`;
    }

    if (result) {
      console.error(result);
      process.exit(1);
    }
  }

  if (!existsSync(config.wasmtime.runtime)) {
    console.error(`Runtime not found: ${config.wasmtime.runtime}`);
    return false;
  }

  if (config.interactive) {
    if (config.tests.updateExpectations) {
      console.error("Can't update test expectations in interactive mode");
      return false;
    }
  }

  return true;
}

let childProcesses = {};

async function run() {
  if (!applyConfig(process.argv)) {
    return process.exit(1);
  }

  let [wptServer, wasmtime] = await Promise.all([ensureWptServer(config.wptServer, config.logLevel),
                                                ensureWasmtime(config.wasmtime, config.logLevel)]);

  if (config.interactive) {
    const server = http.createServer((req, res) => handleRequest(req, res, wasmtime));
    server.listen(config.server.port);
    console.log(`Listening on http://localhost:${config.server.port}`);
  } else {
    let {testPaths, totalCount } = getTests(config.tests.pattern);
    let pathLength = testPaths.reduce((length, path) => Math.max(path.length, length), 0);

    console.log(`Running ${testPaths.length} of ${totalCount} tests ...\n`);

    let expectationsUpdated = 0;
    let unexpectedFailure = false;

    let stats = await runTests(testPaths, wasmtime,
      (testPath, results, stats) => {
        console.log(`${testPath.padEnd(pathLength)} ${formatStats(stats)}`);
        if (config.tests.updateExpectations && stats.unexpectedFail + stats.unexpectedPass + stats.missing > 0) {
          let expectPath = path.join(config.tests.expectations, testPath + ".json");
          console.log(`writing changed expectations to ${expectPath}`);
          let expectations = {};
          for (let result of results) {
            expectations[result.name] = {
              status: result.status === 0 ? 'PASS' : 'FAIL',
            };
          }

          mkdirSync(path.dirname(expectPath), { recursive: true });
          writeFileSync(expectPath, JSON.stringify(expectations, null, 2));
          expectationsUpdated++;
        }
      },
      (testPath, error, stats) => {
        let expectPath = path.join(config.tests.expectations, testPath + ".json");
        let exists = existsSync(expectPath);
        if (exists) {
          console.log(`UNEXPECTED ERROR: ${testPath} (${stats.duration}ms)
  MESSAGE: ${error.message}
  STACK:
  ${error.stack.split('\n').join('\n  ')}`);
          if (config.tests.updateExpectations) {
            console.log(`Removing expectations file ${expectPath}`);
            rmSync(expectPath);
            expectationsUpdated++;
          } else {
            unexpectedFailure = true;
          }
        } else {
          console.log(`EXPECTED ERROR: ${testPath} (${stats.duration}ms)`);
        }
      }
    );

    console.log(`\n${"Done. Stats:".padEnd(pathLength)} ${formatStats(stats)}`);

    shutdown();

    if (config.tests.updateExpectations) {
      console.log(`Expectations updated: ${expectationsUpdated}`);
    } else if (stats.unexpectedFail + stats.unexpectedPass != 0 || unexpectedFailure) {
      process.exitCode = 1;
    }
    return process.exit();
  }
}

function formatStats(stats) {
  return `${padStart(stats.pass, 4)} / ${padStart(stats.count, 4)} (${padStart("+" + stats.unexpectedPass, 5)}, ${padStart("-" + (stats.unexpectedFail), 5)}, ${padStart("?" + (stats.missing), 5)}) passing in ${padStart(stats.duration, 4)}ms`;
}
function padStart(value, length) {
  return (value + "").padStart(length);
}

async function ensureWptServer(config, logLevel) {
  if (config.external) {
    let wptServer = { ...config };
    if (logLevel > LogLevel.Quiet) {
      console.info(`Using external WPT server`);
    }
    return wptServer;
  } else {
    return await startWptServer(config.root, logLevel);
  }
}

async function startWptServer(root, logLevel) {
  if (logLevel > LogLevel.Quiet) {
    console.info(`Starting WPT server (cmd: ${root}/wpt --no-h2 serve)...`);
  }
  let server = execFile(`${root}/wpt`, ["--no-h2", "serve"]);
  childProcesses.wptServer = server;
  server.on("error", event => {
    shutdown(`error starting WPT server: ${event}`);
  });

  if (logLevel >= LogLevel.VeryVerbose) {
    server.stderr.on("data", data => {
      console.log(`WPT server stderr: ${stripTrailingNewline(data)}`);
    });
    server.stdout.on("data", data => {
      console.log(`WPT server stdout: ${stripTrailingNewline(data)}`);
    });
  }


  // Wait until the server has fully initialized.
  // `wpt.py serve` doesn't explicitly signal when it's done initializing, so we have to
  // read the tea leaves a bit, by waiting for a message that is among the very last to be
  // printed during initialization, well after the main http server has started.
  for await(let [chunk] of on(server.stdout, "data")) {
    if (/wss on port \d+\] INFO - Listen on/.test(chunk)) {
      if (logLevel > LogLevel.Quiet) {
        console.info(`Started internal WPT server`);
      }
      return { process: server, ...config };
    }
  }
}

async function ensureWasmtime(config, logLevel) {
  if (config.external) {
    let wasmtime = { ...config, host: `http://${config.addr}/` };
    if (logLevel > LogLevel.Quiet) {
      console.info(`Using external Wasmtime host ${wasmtime.host}`);
    }
    return wasmtime;
  } else {
    let wasmtime = await startWasmtime(config.runtime, config.addr, logLevel);
    if (logLevel > LogLevel.Quiet) {
      console.info(`Started internal Wasmtime host ${wasmtime.host}`);
    }
    return wasmtime;
  }
}

async function timeout(millis, message) {
  if (message === undefined) {
    message = `timeout reached after ${millis} milliseconds`;
  }

  return new Promise((_resolve, reject) => setTimeout(() => reject(message), millis));
}

async function wasmtimeReady(wasmtime, config) {
  // Wait until Wasmtime has fully initialized and extract host from output.
  for await(const [chunk] of on(wasmtime.stderr, "data")) {
    let result = chunk.match(/Serving HTTP on (.+)/);
    if (result) {
      return { process: wasmtime, host: result[1], ...config };
    }
  }
}

async function startWasmtime(runtime, addr, logLevel) {
  if (logLevel > LogLevel.Quiet) {
    console.info(`Starting Wasmtime server (cmd: wasmtime serve -S common --addr ${addr} ${runtime})...`);
  }
  let wasmtime = execFile("wasmtime", ["serve", "-S", "common", "--addr", addr, runtime]);
  childProcesses.wasmtime = wasmtime;
  wasmtime.on("error", event => {
    shutdown(`error starting Wasmtime: ${event}`);
  });

  let backtrace = "";
  let backtrace_re = /Error \{\s+context: "error while executing at wasm backtrace:(.+?)",\s+source: "(.+?)"/s;
  wasmtime.stderr.on("data", data => {
    if (backtrace.length > 0 || data.includes("error while executing at wasm backtrace:")) {
      backtrace += data;
      let match = backtrace.match(backtrace_re);
      if (match) {
        backtrace = "";
        let bt = match[1].split('\\n').join('\n');
        bt = bt.replace(/at \/.+?\js\/src\//g, "at js/src/");
        console.error(`wasmtime stderr: Content panic with stack:${bt}\n\nreason: ${match[2]}\n`);
      }
    } else {
      console.error(`wasmtime stderr: ${stripTrailingNewline(data)}`);
    }
  });

  if (logLevel >= LogLevel.VeryVerbose) {
    wasmtime.stdout.on("data", data => {
      console.log(`wasmtime stdout: ${stripTrailingNewline(data)}`);
    });
  }

  // give wasmtime 20 seconds to become available
  const WASMTIME_READY_TIMEOUT = 20000;
  return await Promise.race([
    wasmtimeReady(wasmtime, config),
    timeout(WASMTIME_READY_TIMEOUT, "Wasmtime failed to start"),
  ]);
}

function stripTrailingNewline(str) {
  if (str[str.length - 1] === '\n') {
    return str.substr(0, str.length - 1);
  }
  return str;
}

function getExpectedResults(testPath) {
  testPath = path.join(config.tests.expectations, testPath + ".json");
  try {
    return JSON.parse(readFileSync(testPath));
  } catch (e) {
    if (config.tests.updateExpectations) {
      if (config.logLevel >= LogLevel.Verbose) {
        console.log(`Expectations file ${testPath} will be created with results from current run`);
      }
    }
    return {};
  }
}

function getTests(pattern) {
  config.logLevel >= LogLevel.Verbose &&
    console.log(`Loading tests list from ${config.tests.list}`);

  let testPaths = JSON.parse(readFileSync(config.tests.list, { encoding: "utf-8" }));
  testPaths = testPaths.filter(path => !path.startsWith(SKIP_PREFIX));
  let totalCount = testPaths.length;
  if (config.skipSlowTests) {
    testPaths = testPaths.filter(path => !path.startsWith(SLOW_PREFIX));
  }
  testPaths = testPaths.map(path => path.startsWith(SLOW_PREFIX) ?
                                    path.substr(SLOW_PREFIX.length) :
                                    path)
    .filter(path => path.indexOf(pattern) != -1);

  config.logLevel >= LogLevel.Verbose &&
    console.log(`Loaded ${totalCount} tests, of which ${testPaths.length} match pattern ${pattern}${config.skipSlowTests ? " and aren't skipped for being slow" : ""}`);
  return { testPaths, totalCount };
}

async function runTests(testPaths, wasmtime, resultCallback, errorCallback) {
  let totalStats = {
    duration: 0,
    count: 0,
    pass: 0,
    missing: 0,
    unexpectedPass: 0,
    unexpectedFail: 0,
  };

  for (let path of testPaths) {
    if (config.logLevel >= LogLevel.Verbose) {
      console.log(`Running test ${path}`);
    }

    let expectations = getExpectedResults(path);
    let t1 = Date.now();
    let response, body;
    try {
      if (config.logLevel >= LogLevel.VeryVerbose) {
        console.log(`Sending request to ${wasmtime.host}${path}`);
      }
    } catch (e) {
      shutdown(`Error while running test ${path}: ${e}`);
    }
    let stats = {
      count: 0,
      pass: 0,
      missing: 0,
      unexpectedPass: 0,
      unexpectedFail: 0,
      duration: Date.now() - t1,
    };
    totalStats.duration += stats.duration;
    let results;
    try {
      response = await fetch(`${wasmtime.host}${path}`);
      body = await response.text();
      results = JSON.parse(body);
      if (response.status == 500) {
        throw {message: results.error.message, stack: results.error.stack};
      }

      for (let result of results) {
        stats.count++;

        let expectation = expectations[result.name];
        if (expectation) {
          expectation.did_run = true;
          result.expected = true;
        }

        if (result.status == 0) {
          stats.pass++;
          if (!expectation || expectation.status === 'FAIL') {
            result.expected = false;
            console.log(`${expectation ? "UNEXPECTED" : "NEW"} PASS
            NAME:    ${result.name}`);
            stats.unexpectedPass++;
          }
        } else if (!expectation || expectation.status === 'PASS') {
          result.expected = false;
          console.log(`${expectation ? "UNEXPECTED" : "NEW"} FAIL
  NAME:    ${result.name}
  MESSAGE: ${result.message}`);
          stats.unexpectedFail++;
        }
      }

      for (let [name, expectation] of Object.entries(expectations)) {
        if (!expectation.did_run) {
          stats.missing++;
          console.log(`MISSING TEST
  NAME:    ${name}
  EXPECTED RESULT: ${expectation.status}`);
        }
      }

      totalStats.count += stats.count;
      totalStats.pass += stats.pass;
      totalStats.missing += stats.missing;
      totalStats.unexpectedPass += stats.unexpectedPass;
      totalStats.unexpectedFail += stats.unexpectedFail;

      await resultCallback(path, results, stats);
    } catch (e) {
      if (!results) {
        e = new Error(`Parsing test results as JSON failed. Output was:\n  ${body}`);
      }
      if (config.logLevel >= LogLevel.Verbose) {
        console.log(`Error running file ${path}: ${e.message}, stack:\n${e.stack}`);
      }
      await errorCallback(path, e, stats);
    }
  }

  return totalStats;
}

async function handleRequest(req, res, wasmtime) {
  let pattern = req.url.substr(1);
  if (pattern == "favicon.ico") {
    return;
  }

  let {testPaths, totalCount } = getTests(pattern);

  res.writeHead(200, { 'Content-Type': 'text/html' });
  let page = readFileSync(config.results.pageTemplate, { encoding: "utf-8" });
  let [pageStart, pageEnd] = page.split("{results}");
  pageStart = pageStart.split("{pattern}").join(`${pattern}`);
  pageStart = pageStart.split("{count}").join(`${testPaths.length} of ${totalCount}`);
  res.write(pageStart);

  let section = readFileSync(config.results.sectionTemplate, { encoding: "utf-8" });
  let template = new Function("prefix", "title", "info", "pass", "total", "duration", "rows", `return \`${section}\``);
  let section_error = readFileSync(config.results.sectionErrorTemplate, { encoding: "utf-8" });
  let error_template = new Function("prefix", "title", "message", "stack", `return \`${section_error}\``);

  let { duration, pass, count } = await runTests(testPaths, wasmtime,
    (testPath, results, stats) => {
      let table = renderResultsTable(testPath, results, stats, template);
      res.write(table);
    },
    (testPath, error, stats) => {
      let expectPath = path.join(config.tests.expectations, testPath + ".json");
      let exists = existsSync(expectPath);
      let table = error_template(`${exists ? "UN" : ""}EXPECTED ERROR: `, testPath, error.message,
                                 renderStack(error.stack));
      res.write(table);
    }
  );

  res.end(pageEnd.split("{pass}").join(pass).split("{total}").join(count).split("{duration}").join(duration));
}

function renderResultsTable(title, results, stats, template) {
  let rows = results.map(test => {
      let name = test.name.split("<").join("&lt;").split(">").join("&gt;");
      return `<tr class="result ${test.status == 0 ? "pass" : "fail"}${test.expected ? " expected" : ""}">
      <td class="name">${name}</td>
      <td class="grade">${test.status == 0 ? "PASS" : "FAIL"}</td>
      <td message>${
        test.status ?
          `<p>${test.message}</p>
          <details>
          <summary>Stack</summary>
          ${renderStack(test.stack)}
          </details>`
        : ""
      }</td>
      `
    }
  ).join("\n");

  return template("", title, "", stats.pass, stats.count, stats.duration, rows);
}

function renderStack(stack) {
  stack = stack.split("<").join("&lt;");
  stack = stack.split(">").join("&gt;");

  // Strip the parts of the stack that's just about handling asserts.
  let lines = stack.split("\n");
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].indexOf("assert_wrapper") > -1) {
      lines = lines.slice(i + 1);
      break;
    }
  }

  stack = lines.join("<br>");
  return stack;
}

process
    .on('SIGTERM', shutdown)
    .on('SIGINT', shutdown)
    .on('uncaughtException', shutdown);

function shutdown(err = undefined) {
  if (err && err !== "SIGINT" && err !== "SIGTERM") {
    console.error(`Exiting with error ${err}`);
    err.stack && console.error(`Stack: ${err.stack}`);
  }
  childProcesses.wptServer && childProcesses.wptServer.kill("SIGINT");
  childProcesses.wasmtime && childProcesses.wasmtime.kill("SIGKILL");
  process.exit(err ? 1 : 0);
}

await run();
