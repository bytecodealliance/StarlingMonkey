# StarlingMonkey Debugger for VS Code

This extension adds support for debugging JavaScript code running in the [StarlingMonkey](https://github.com/bytecodealliance/StarlingMonkey/) JS runtime inside a [WebAssembly Component](https://component-model.bytecodealliance.org/).

**NOTE:** This extension is experimental for now, and will break in surprising and not always enjoyable ways.

## Using The StarlingMonkey Debugger

The extension is currently experimental and can't be installed directly. Instead, follow the instructions below for building it:

## Building and Running

* Install a recent version of the [Wasmtime](https://wasmtime.dev/) WebAssembly runtime and ensure that it's available in your path[^1]
* Clone the [StarlingMonkey project](https://github.com/bytecodealliance/StarlingMonkey/)
* Ensure that you have a branch checked out that contains the debugger
* Open the contained folder `debugger/vscode-dap-extension` in VS Code
* Ensure that a build of StarlingMonkey is available with the path `debugger/vscode-dap-extension/out/main.wasm`. This is configurable via the `component` property of the launch configuration in `launch.json`[^2]
* Open the "Run and Debug" sidebar and ensure that the `Extension` launch configuration is active
* Press `F5` to build the extension and launch another VS Code window with it installed
* In the explorer view of the new window open `main.js`
* Set some breakpoints
* Press `F5` to start debugging the file
* invoke the component. For HTTP server components, this would be done by sending an HTTP request to the address the component is served at. By default, that would be `http://localhost:8080/`

## Configuration

The extension adds a `StarlingMonkey Debugger` section to VS Code's configuration. Open the VS Code settings and search for `StarlingMonkey` to see the available options.

[^1]: Alternatively, you can use another WebAssembly Components runtime, as long as it supports outgoing TCP socket connections, and passing environment variables to the guest. In that case, you'll have to update the [configuration](#configuration) to ensure the right options are passed to the runtime.
[^2]: The currently easiest way to get this file is to build StarlingMonkey itself using the CMake target `starling`, and then copying the resulting `starling.wasm` file from the CMake build folder. See the repository's top-level `README` for details on the build process
