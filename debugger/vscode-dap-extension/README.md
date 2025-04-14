# StarlingMonkey Debugger for VS Code

This extension adds support for debugging JavaScript code running in the [StarlingMonkey](https://github.com/bytecodealliance/StarlingMonkey/) JS runtime inside a [WebAssembly Component](https://component-model.bytecodealliance.org/).

**NOTE:** This extension is experimental for now, and will break in surprising and not always enjoyable ways.

## Using The StarlingMonkey Debugger

The extension works for content built with a recent (`0.18` and up) version of [ComponentizeJS](https://www.npmjs.com/package/@bytecodealliance/componentize-js), provided some requirements are met.

### Building Content

The content needs to be invoked via the `wasi:http/incoming-handler@0.2.*/handle` function. In other words, it needs to target the `wasi:http/proxy@0.2` world.

Additionally, it needs to import a few additionally interfaces, which you can add to your `*.wit` by adding these lines:

```wit
  import wasi:cli/environment@0.2.3;
  import wasi:sockets/network@0.2.3;
  import wasi:sockets/instance-network@0.2.3;
  import wasi:sockets/tcp@0.2.3;
  import wasi:sockets/tcp-create-socket@0.2.3;
```

And finally, when invoking `componentize-js`, the arguments `--runtime-args "--enable-script-debugging"` need to be passed.

### Running Content

To debug content, you need to create a launch config of the type `starlingmonkey`. By default, this will start the [Wasmtime](https://wasmtime.dev/) WebAssembly runtime with the provided component.

Once `wasmtime` is running, sending a request to the host/port it's running on will cause StarlingMonkey to connect to the debugger.

## Configuration

The extension adds a `StarlingMonkey Debugger` section to VS Code's configuration. Open the VS Code settings and search for `StarlingMonkey` to see the available options.

Additionally, all settings in this section can be overridden in the launch configuration.

[^1]: Alternatively, you can use another WebAssembly Components runtime, as long as it supports outgoing TCP socket connections, and passing environment variables to the guest. In that case, you'll have to update the [configuration](#configuration) to ensure the right options are passed to the runtime.
