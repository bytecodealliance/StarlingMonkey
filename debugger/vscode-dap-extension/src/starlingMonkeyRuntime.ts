import { Scope } from "@vscode/debugadapter";
import { ChildProcessWithoutNullStreams, spawn } from "child_process";
import { EventEmitter } from "events";
import * as Net from "net";
import { Signal } from "./signals.js";
import { assert } from "console";

export interface FileAccessor {
  isWindows: boolean;
  readFile(path: string): Promise<Uint8Array>;
  writeFile(path: string, contents: Uint8Array): Promise<void>;
}

export interface IRuntimeBreakpoint {
  id: number;
  line: number;
  column: number;
}

interface IRuntimeStackFrame {
  index: number;
  name: string;
  file: string;
  line: number;
  column?: number;
  instruction?: number;
}

interface IRuntimeStack {
  count: number;
  frames: IRuntimeStackFrame[];
}

interface IRuntimeMessage {
  type: string;
  value: any;
}

interface IRuntimeVariable {
  name: string;
  value: string;
  type: string;
  variablesReference: number;
}

export interface IComponentRuntimeConfig {
  executable: string;
  options: string[];
  envOption: string;
}
export interface IStarlingMonkeyRuntimeConfig {
  jsRuntimeOptions: string[];
  componentRuntime: IComponentRuntimeConfig;
}

class ComponentRuntimeInstance {
  private static _componentRuntime: ChildProcessWithoutNullStreams;
  static running: boolean;
  static workspaceFolder: string;
  static _server: Net.Server;
  static _nextSessionPort: number | undefined;

  static setNextSessionPort(port: number) {
    this._nextSessionPort = port;
  }

  static async start(workspaceFolder: string, component: string, config: IStarlingMonkeyRuntimeConfig) {
    assert(!this.running, "ComponentRuntime is already running");
    this.running = true;
    this.workspaceFolder = workspaceFolder;

    this._server = Net.createServer((socket) => {
      socket.on("data", (data) => {
        assert(
          data.toString() === "get-session-port",
          `expected "get-session-port" message, got "${data.toString()}"`
        );
        console.debug("StarlingMonkey sent a get-session-port request");
        if (!this._nextSessionPort) {
          console.debug(
            "No debugging session active, telling runtime to continue"
          );
          socket.write("no-session");
        } else {
          console.debug(
            `Starting debug session on port ${this._nextSessionPort}`
          );
          socket.write(`${this._nextSessionPort}`);
          this._nextSessionPort = undefined;
        }
      });
    }).listen();
    let port = (<Net.AddressInfo>this._server.address()).port;
    console.info(`waiting for debug protocol on port ${port}`);

    // Start componentRuntime as a new process
    let componentRuntimeArgs = Array.from(config.componentRuntime.options).map(opt => {
      return opt
        .replace("${workspaceFolder}", workspaceFolder)
        .replace("${component}", component);
    });
    componentRuntimeArgs.push(config.componentRuntime.envOption);
    componentRuntimeArgs.push(
      `STARLINGMONKEY_CONFIG="${config.jsRuntimeOptions.join(" ")}"`
    );
    componentRuntimeArgs.push(config.componentRuntime.envOption);
    componentRuntimeArgs.push(`DEBUGGER_PORT=${port}`);
    console.debug(
      `${config.componentRuntime.executable} ${componentRuntimeArgs.join(" ")}`
    );
    this._componentRuntime = spawn(
      config.componentRuntime.executable,
      componentRuntimeArgs,
      { cwd: workspaceFolder }
    );

    this._componentRuntime.stdout.on("data", (data) => {
      console.log(`componentRuntime ${data}`);
    });

    this._componentRuntime.stderr.on("data", (data) => {
      console.error(`componentRuntime ${data}`);
    });

    this._componentRuntime.on("close", (code) => {
      console.info(`child process exited with code ${code}`);
      this.running = false;
    });
  }
}

export class StarlingMonkeyRuntime extends EventEmitter {
  private _debug!: boolean;
  private _stopOnEntry!: boolean;
  public get fileAccessor(): FileAccessor {
    return this._fileAccessor;
  }
  public set fileAccessor(value: FileAccessor) {
    this._fileAccessor = value;
  }

  private _server!: Net.Server;
  private _socket!: Net.Socket;

  private _messageReceived = new Signal<IRuntimeMessage, void>();

  private _sourceFile!: string;
  public get sourceFile() {
    return this._sourceFile;
  }

  constructor(
    private _workspaceDir: string,
    private _fileAccessor: FileAccessor,
    private _config: IStarlingMonkeyRuntimeConfig
  ) {
    super();
  }

  public async start(program: string, component: string, stopOnEntry: boolean, debug: boolean  ): Promise<void> {
    await this.startComponentRuntime(component);
    this.startSessionServer();
    // TODO: tell StarlingMonkey not to debug if this is false.
    this._debug = debug;
    this._stopOnEntry = stopOnEntry;
    this._sourceFile = this.normalizePath(program);
    let message = await this._messageReceived.wait();
    assert(
      message.type === "connect",
      `expected "connect" message, got "${message.type}"`
    );
    this.sendMessage("startDebugLogging");
    message = await this.sendAndReceiveMessage("loadProgram", this._sourceFile);
    assert(
      message.type === "programLoaded",
      `expected "programLoaded" message, got "${message.type}"`
    );
    this.emit("programLoaded");
  }

  /**
   * Starts a server that creates a new session for every connection request.
   *
   * The server listens for incoming connections and handles data received from the client.
   * It attempts to parse the received data as JSON and resolves the `_messageReceived` promise
   * with the parsed message. If the data cannot be parsed, it is stored in `partialMessage`
   * for the next data event.
   *
   * When the connection ends, the server emits an "end" event.
   *
   * The server listens on a dynamically assigned port, which is then set as the next session port
   * in the `ComponentRuntimeInstance`.
   */
  startSessionServer(): void {
    let debuggerScriptSent = false;
    let partialMessage = "";
    let expectedLength = 0;
    let eol = -1;
    let lengthReceived = false;

    async function resetMessageState() {
      partialMessage = partialMessage.slice(expectedLength);
      expectedLength = 0;
      lengthReceived = false;
      eol = -1;
      if (partialMessage.length > 0) {
        await 1; // Ensure that the current message is processed before the next data event.
        handleData("");
      }
    }

    const handleData = async (data: string) => {
      if (!debuggerScriptSent) {
        if (data.toString() !== "get-debugger") {
          console.warn(
            `expected "get-debugger" message, got "${data.toString()}". Ignoring ...`
          );
          return;
        }
        let extensionDir = `${__dirname}/../`;
        let debuggerScript = await this._fileAccessor.readFile(`${extensionDir}/dist/debugger.js`);
        this._socket.write(`${debuggerScript.length}\n${debuggerScript}`);
        debuggerScriptSent = true;
        return;
      }

      partialMessage += data;

      if (!lengthReceived) {
        eol = partialMessage.indexOf("\n");
        if (eol === -1) {
          return;
        }
        lengthReceived = true;
        expectedLength = parseInt(partialMessage.slice(0, eol), 10);
        if (isNaN(expectedLength)) {
          console.warn(`expected message length, got "${partialMessage}"`);
          resetMessageState();
          return;
        }
        partialMessage = partialMessage.slice(eol + 1);
        lengthReceived = true;
      }
      if (partialMessage.length < expectedLength) {
        return;
      }
      let message = partialMessage.slice(0, expectedLength);
      try {
        let parsed = JSON.parse(message);
        console.debug(`received message ${partialMessage}`);
        resetMessageState();
        this._messageReceived.resolve(parsed);
      } catch (e) {
        console.warn(`Illformed message received. Error: ${e}, message: ${partialMessage}`);
        resetMessageState();
      }
    }

    this._server = Net.createServer(socket => {
      this._socket = socket;
      console.debug("Debug session server accepted connection from client");
      socket.on("data", handleData);
      socket.on("end", () => this.emit("end"));
    }).listen();
    let port = (<Net.AddressInfo>this._server.address()).port;
    ComponentRuntimeInstance.setNextSessionPort(port);
  }

  private async startComponentRuntime(component: string) {
    if (ComponentRuntimeInstance.running) {
      assert(
        ComponentRuntimeInstance.workspaceFolder === this._workspaceDir,
        "ComponentRuntime is already running in a different workspace"
      );
      return;
    }
    await ComponentRuntimeInstance.start(this._workspaceDir, component, this._config);
  }

  private sendMessage(type: string, value?: any, useRawValue = false) {
    let message: string;
    if (useRawValue) {
      message = `{"type": "${type}", "value": ${value}}`;
    } else {
      message = JSON.stringify({ type, value });
    }
    console.debug(`sending message to runtime: ${message}`);
    this._socket.write(`${message.length}\n${message}`);
  }

  private sendAndReceiveMessage(
    type: string,
    value?: any,
    useRawValue = false
  ): Promise<any> {
    this.sendMessage(type, value, useRawValue);
    return this._messageReceived.wait();
  }

  public async run() {
    if (this._debug && this._stopOnEntry) {
      this.emit("stopOnEntry");
    } else {
      this.continue();
    }
  }

  public async continue() {
    let message = await this.sendAndReceiveMessage("continue");
    // TODO: handle other results, such as run to completion
    assert(
      message.type === "breakpointHit",
      `expected "breakpointHit" message, got "${message.type}"`
    );
    this.emit("stopOnBreakpoint");
  }

  public next(granularity: "statement" | "line" | "instruction") {
    this.handleStep("next");
  }

  public stepIn(targetId: number | undefined) {
    this.handleStep("stepIn");
  }

  public stepOut() {
    this.handleStep("stepOut");
  }

  private async handleStep(type: "next" | "stepIn" | "stepOut") {
    let message = await this.sendAndReceiveMessage(type);
    // TODO: handle other results, such as run to completion
    assert(
      message.type === "stopOnStep",
      `expected "stopOnStep" message, got "${message.type}"`
    );
    this.emit("stopOnStep");
  }

  public async stack(index: number, count: number): Promise<IRuntimeStack> {
    let message = await this.sendAndReceiveMessage("getStack", {
      index,
      count,
    });
    assert(
      message.type === "stack",
      `expected "stack" message, got "${message.type}"`
    );
    let stack = message.value;
    for (let frame of stack) {
      frame.file = this.qualifyPath(frame.file);
    }
    return {
      count: stack.length,
      frames: stack,
    };
  }

  async getScopes(frameId: number): Promise<Scope[]> {
    let message = await this.sendAndReceiveMessage("getScopes", frameId);
    assert(
      message.type === "scopes",
      `expected "scopes" message, got "${message.type}"`
    );
    return message.value;
  }

  public async getBreakpointLocations(
    path: string,
    line: number
  ): Promise<{ line: number; column: number }[]> {
    // TODO: support the full set of query params from BreakpointLocationsArguments
    path = this.normalizePath(path);
    let message = await this.sendAndReceiveMessage("getBreakpointsForLine", {
      path,
      line,
    });
    assert(
      message.type === "breakpointsForLine",
      `expected "breakpointsForLine" message, got "${message.type}"`
    );
    return message.value;
  }

  public async setBreakPoint(
    path: string,
    line: number,
    column?: number
  ): Promise<IRuntimeBreakpoint> {
    path = this.normalizePath(path);
    let response = await this.sendAndReceiveMessage("setBreakpoint", {
      path,
      line,
      column,
    });
    assert(
      response.type === "breakpointSet",
      `expected "breakpointSet" message, got "${response.type}"`
    );
    return response.value;
  }

  public async getVariables(reference: number): Promise<IRuntimeVariable[]> {
    let message = await this.sendAndReceiveMessage("getVariables", reference);
    assert(
      message.type === "variables",
      `expected "variables" message, got "${message.type}"`
    );
    return message.value;
  }

  public async setVariable(
    variablesReference: number,
    name: string,
    value: string
  ): Promise<IRuntimeVariable> {
    // Manually encode the value so that it'll be decoded as raw values by the runtime, instead of everything becoming a string.
    let rawValue = `{"variablesReference": ${variablesReference}, "name": "${name}", "value": ${value}}`;
    let message = await this.sendAndReceiveMessage(
      "setVariable",
      rawValue,
      true
    );
    assert(
      message.type === "variableSet",
      `expected "variableSet" message, got "${message.type}"`
    );
    return message.value;
  }

  // private methods
  private normalizePath(path: string) {
    path = path.replace(/\\/g, "/");
    return path.startsWith(this._workspaceDir)
      ? path.substring(this._workspaceDir.length + 1)
      : path;
  }

  private qualifyPath(path: string) {
    return `${this._workspaceDir}/${path}`;
  }
}
