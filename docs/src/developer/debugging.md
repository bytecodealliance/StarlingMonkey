# Debugging a StarlingMonkey application

StarlingMonkey is a JavaScript engine -- based on  SpiderMonkey -- that is designed to be lightweight and efficient for compilation to WebAssembly components. To debug StarlingMonkey components means the component must implement some projection of the StarlingMonkey debugger API, as the WebAssembly sandbox prevents any access to internal code that isn't formally exposed.

This can be done with a custom WIT-defined API based directly on StarlingMonkey's debugger API, but for flexibility with different IDEs it makes sense to start with a debugging server approach in which a debug-build of the component adds the networking interfaces required to communicate with Visual Studio Code using the Debugger Application Protocol, or _DAP_. 

## Visual Studio Code StarlingMonkey Debugger Extension

The Bytecode Alliance Foundation publishes a Visual Studio Code extension for debugging StarlingMonkey applications. This extension provides a user-friendly interface for setting breakpoints, inspecting variables, and controlling the execution flow of StarlingMonkey code running in a WebAssembly environment created using [componentize-js](https://github.com/bytecodealliance/componentize-js). 

The steps to use this extension in VSCode are:
1. Download and install the [StarlingMonkey Debugger extension](https://marketplace.visualstudio.com/items?itemName=BytecodeAlliance.starlingmonkey-debugger). 
2. Open your StarlingMonkey project in VSCode and create a launch.json file that follows [this configuration pattern](https://marketplace.visualstudio.com/items?itemName=BytecodeAlliance.starlingmonkey-debugger#running-content).
3. Make a copy of the `wit` folder to a folder named `wit-debug`. In that `wit-debug` folder, rename the the world .wit file to add `-debug` to the filename and include the following extra interfaces that support the debug connection from StarlingMonkey to Visual Studio Code:
```bash
import wasi:cli/environment@0.2.3;
import wasi:sockets/network@0.2.3;
import wasi:sockets/instance-network@0.2.3;
import wasi:sockets/tcp@0.2.3;
import wasi:sockets/tcp-create-socket@0.2.3;
```
4. Retarget the debug build command to be of the form: `npm run bundle && componentize-js --use-debug-build --runtime-args \"--enable-script-debugging\" --wit <wit-debug> -o dist/$npm_package_name.wasm dist/bundle.js` where the `-o` path is to the bundle file name that is specified in the `rollup.config.js` file. 

An example may be found in the https://github.com/bytecodealliance/sample-wasi-http-js/package.json file.

Currently, the StarlingMonkey Debugger extension is in active development. (The objective is to use this extension as a generalizable mechanism for any language debugger that requires a debug server to project itself to Visual Studio Code.)