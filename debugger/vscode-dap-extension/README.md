# StarlingMonkey Debugger for VS Code

This extension adds support for debugging JavaScript code running in the [StarlingMonkey](https://github.com/bytecodealliance/StarlingMonkey/) JS runtime inside a [WebAssembly Component](https://component-model.bytecodealliance.org/).

**NOTE:** This extension is experimental for now, and will break in surprising and not always enjoyable ways.

## Using The StarlingMonkey Debugger

The extension works for content built with a recent (`0.18` and up) version of [ComponentizeJS](https://www.npmjs.com/package/@bytecodealliance/componentize-js), provided some requirements are met.

### Building Content

Build using `componentize-js`.  You will need to `npm install -g @bytecodealliance/componentize-js`.

When invoking `componentize-js`, you **must** pass the `--runtime-args "--enable-script-debugging"` flag.

**JavaScript**

`componentize-js --wit world.wit -o main.wasm --runtime-args "--enable-script-debugging" main.js`

**TypeScript**

In `package.json`:

```
"scripts": {
    "compile": "mkdirp out && tsc && componentize-js --wit world.wit -o out/main2.wasm --runtime-args \"--enable-script-debugging\" dist/main2.js"
}
```

then `npm run compile`

## Running Content

To debug content, you need to create a launch config of the type `starlingmonkey`. By default, this will start the [Wasmtime](https://wasmtime.dev/) WebAssembly runtime with the provided component.

Example launch config:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "starlingmonkey",
      "request": "launch",
      "name": "Debug StarlingMonkey component",
      "component": "${workspaceFolder}/main.wasm",
      "program": "${workspaceFolder}/main.js",
      "stopOnEntry": false,
      "trace": true
    }
  ]
}
```

Once `wasmtime` is running, sending a request to the host/port it's running on will cause StarlingMonkey to connect to the debugger.

## Configuration

The extension adds a `StarlingMonkey Debugger` section to VS Code's configuration. Open the VS Code settings and search for `StarlingMonkey` to see the available options.

Additionally, all settings in this section can be overridden in the launch configuration.

[^1]: Alternatively, you can use another WebAssembly Components runtime, as long as it supports outgoing TCP socket connections, and passing environment variables to the guest. In that case, you'll have to update the [configuration](#configuration) to ensure the right options are passed to the runtime.

## Extension Developer Info

### Testing

Use the `tests` directory (and add to it!).  Build commands:

**`js` workspace:** `componentize-js --wit world.wit -o main.wasm --runtime-args "--enable-script-debugging" main.js`

**`ts` workspace:** `npm run compile`
