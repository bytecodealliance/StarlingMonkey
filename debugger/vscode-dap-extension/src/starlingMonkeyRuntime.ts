import { Scope } from "@vscode/debugadapter";
import { EventEmitter } from "events";
import * as Net from "net";
import { Signal } from "./signals.js";
import { assert } from "console";
import { Terminal, TerminalShellExecution, window } from "vscode";

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
  private static _workspaceFolder?: string;
  private static _component?: string;
  private static _server?: Net.Server;
  private static _terminal?: Terminal;
  private static _nextSessionPort?: number;
  private static _runtimeExecution?: TerminalShellExecution;

  static setNextSessionPort(port: number) {
    this._nextSessionPort = port;
  }

  static async start(workspaceFolder: string, component: string, config: IStarlingMonkeyRuntimeConfig) {
    if (this._workspaceFolder && this._workspaceFolder !== workspaceFolder ||
        this._component && this._component !== component
    ) {
      this.reset();
    }

    this._workspaceFolder = workspaceFolder;
    this._component = component;

    this.ensureServer();
    this.ensureHostRuntime(config, workspaceFolder, component);
  }
  static reset() {
    this._workspaceFolder = undefined;
    this._component = undefined;
    this._nextSessionPort = undefined;
    this._server?.close();
    this._server = undefined;
    this._terminal?.dispose();
    this._terminal = undefined;
    this._runtimeExecution = undefined;
  }

  static ensureServer() {
    if (this._server) {
      return;
    }
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
          socket.write(`${this._nextSessionPort}\n`);
          this._nextSessionPort = undefined;
        }
      });
      socket.on("close", () => {
        console.debug("ComponentRuntime disconnected");
      });
    }).listen();
  }

  private static serverPort() {
    return (<Net.AddressInfo>this._server!.address()).port;
  }

  private static async ensureHostRuntime(config: IStarlingMonkeyRuntimeConfig, workspaceFolder: string, component: string) {
    if (this._runtimeExecution) {
      return;
    }

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
    componentRuntimeArgs.push(`DEBUGGER_PORT=${this.serverPort()}`);

    console.debug(
      `${config.componentRuntime.executable} ${componentRuntimeArgs.join(" ")}`
    );

    await this.ensureTerminal();

    if (this._terminal!.shellIntegration) {
      this._runtimeExecution = this._terminal!.shellIntegration.executeCommand(
        config.componentRuntime.executable,
        componentRuntimeArgs
      );
      let disposable = window.onDidEndTerminalShellExecution((event) => {
        if (event.execution === this._runtimeExecution) {
          this._runtimeExecution = undefined;
          disposable.dispose();
          console.log(`Component host runtime exited with code ${event.exitCode}`);
        }
      });
    } else {
      // Fallback to sendText if there is no shell integration.
      // Send Ctrl+C to kill any existing component runtime first.
      this._terminal!.sendText('\x03', false);
      this._terminal!.sendText(
        `${config.componentRuntime.executable} ${componentRuntimeArgs.join(" ")}`,
        true
      );
    }
  }

  private static async ensureTerminal() {
    if (this._terminal && this._terminal.exitStatus === undefined) {
      return;
    }

    let signal = new Signal<void, void>();
    this._terminal = window.createTerminal();
    let terminalCloseDisposable = window.onDidCloseTerminal((terminal) => {
      if (terminal === this._terminal) {
        signal.resolve();
        this._terminal = undefined;
        this._runtimeExecution = undefined;
        terminalCloseDisposable.dispose();
      }
    });

    let shellIntegrationDisposable = window.onDidChangeTerminalShellIntegration(
      async ({ terminal }) => {
        if (terminal === this._terminal) {
          clearTimeout(timeout);
          shellIntegrationDisposable.dispose();
          signal.resolve();
        }
      }
    );
    // Fallback to sendText if there is no shell integration within 3 seconds of launching
    let timeout = setTimeout(() => {
      shellIntegrationDisposable.dispose();
      signal.resolve();
    }, 3000);

    await signal.wait();
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
    await ComponentRuntimeInstance.start(this._workspaceDir, component, this._config);
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
    // this.sendMessage("startDebugLogging");
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
        // console.debug(`received message ${partialMessage}`);
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

  private sendMessage(type: string, value?: any, useRawValue = false) {
    let message: string;
    if (useRawValue) {
      message = `{"type": "${type}", "value": ${value}}`;
    } else {
      message = JSON.stringify({ type, value });
    }
    // console.debug(`sending message to runtime: ${message}`);
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
