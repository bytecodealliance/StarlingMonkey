import { Scope } from "@vscode/debugadapter";
import { EventEmitter } from "events";
import * as Net from "net";
import * as Path from "path";
import { Signal } from "./signals.js";
import { Terminal, TerminalShellExecution, window } from "vscode";
import { SourceLocation, SourceMaps } from "./sourcemaps/sourceMaps.js";
import { dirname } from "path";
import { BreakpointSetResponse, BreakpointsForLineResponse, EvaluateResponse, InstanceToRuntimeMessage, IRuntimeBreakpoint, IRuntimeStackFrame, IRuntimeVariable, RuntimeToInstanceMessage, ScopesResponse, StackResponse, VariableSetResponse, VariablesResponse } from '../shared/messages';

export interface FileAccessor {
  isWindows: boolean;
  readFile(path: string): Promise<Uint8Array>;
  writeFile(path: string, contents: Uint8Array): Promise<void>;
}

interface IRuntimeStack {
  count: number;
  frames: ReadonlyArray<IRuntimeStackFrame>;
}

// TODO: not sure where this is coming from
type OutputType = 'prio' | 'out' | 'err' | 'console';

// Events raised from the SMRuntime to the SMDebugger (via `emit`, so
// in-process).
type RuntimeEventMap = {
  programLoaded: [],
  output: [type: OutputType, text: string, filePath: string, line: number, column: number],
  stopOnEntry: [],
  stopOnBreakpoint: [],
  // TODO: are these used? Does SM support them?
  stopOnDataBreakpoint: [],
  stopOnInstructionBreakpoint: [],
  stopOnException: [exception: any | undefined],
  stopOnStep: [],
  end: [],
}

// TODO: do we need a 'paused' state, for when we are at a breakpoint?
// Running seems to adequately cover it but I am not sure if there are
// actions/messagest that should only be available when paused (e.g.
// get stack, get-set variables).
type RuntimeState =
  | Initialising
  | Connecting
  | LoadingScript
  | Running;

interface Initialising {
  state: 'init';
}
interface Connecting {
  state: 'connecting';
} 
interface LoadingScript {
  state: 'loadingScript';
}
interface Running {
  state: 'running';
}

function assert(condition: any, msg?: string): asserts condition {
  console.assert(condition, msg);
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

export class StarlingMonkeyRuntime extends EventEmitter<RuntimeEventMap> {
  private _debug!: boolean;
  private _stopOnEntry!: boolean;
  private _sourceMaps!: SourceMaps;
  public get fileAccessor(): FileAccessor {
    return this._fileAccessor;
  }
  public set fileAccessor(value: FileAccessor) {
    this._fileAccessor = value;
  }

  private _server!: Net.Server;
  private _socket!: Net.Socket;

  private _state: RuntimeState = { state: 'init' };
  private _messageReceived = new Signal<InstanceToRuntimeMessage, void>();

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
    this._state = { state: 'connecting' };
    await ComponentRuntimeInstance.start(this._workspaceDir, component, this._config);
    this.startSessionServer();
    // TODO: tell StarlingMonkey not to debug if this is false.
    this._debug = debug;
    this._stopOnEntry = stopOnEntry;
    this._sourceFile = this.normalizePath(program);

    this.runDebugLoop();
  }

  async runDebugLoop() {
    // We have a slightly odd mix here of state machine stuff (unsolicited messages)
    // and request-response stuff.  Feels like there should be a better way.

    while (true) {
      let message = await this._messageReceived.wait();

      // Deal with request-response messages
      // TODO: I still feel like this would be safer with correlation IDs but :shrug: for now
      if (message.type === 'breakpointSet') {
        this._bpSet.resolve(message);
        continue;
      }
      if (message.type === 'variables') {
        this._variables.resolve(message);
        continue;
      }
      if (message.type === 'variableSet') {
        this._variableSet.resolve(message);
        continue;
      }
      if (message.type === 'evaluate') {
        this._eval.resolve(message);
        continue;
      }
      if (message.type === 'stack') {
        this._stack.resolve(message);
        continue;
      }
      if (message.type === 'scopes') {
        this._scopes.resolve(message);
        continue;
      }
      if (message.type === 'breakpointsForLine') {
        this._bpsForLine.resolve(message);
        continue;
      }

      switch (this._state.state) {
        case 'init':
          console.warn(`unexpected message '${message.type}' during init - ignored`);
          break;
        case 'connecting':
          switch (message.type) {
            case 'connect':
              console.debug(`connected to SM host (received '${message.type}' message)`);
              this.sendMessage({ type: "loadProgram", value: this._sourceFile });
              this._state = { state: 'loadingScript' };
              break;
            default:
              console.warn(`unexpected message '${message.type}' during ${this._state.state} - ignored`);
              break;
          }
          break;
        case 'loadingScript':
          switch (message.type) {
            case 'programLoaded':
              console.debug(`loaded debugger script into SM host (received '${message.type}' message)`);
              this.initSourceMaps(message.value);
              this._state = { state: 'running' };
              this.emit('programLoaded');
              break;
            default:
              console.warn(`unexpected message '${message.type}' during ${this._state.state} - ignored`);
              break;
          }
          break;
        case 'running':
          switch (message.type) {
            case 'breakpointHit':
              this.emit('stopOnBreakpoint');
              break;
            case 'stopOnStep':
              this.emit('stopOnStep');
              break;
            default:
              console.warn(`unexpected message '${message.type}' during ${this._state.state} - ignored`);
              break;
          }
          break;
      }
    }
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
      console.debug(`<-- received: ${message}`);
      try {
        let parsed = JSON.parse(message);
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

  private sendMessage(message: RuntimeToInstanceMessage) {
    const json = JSON.stringify(message);
    this._socket.write(`${json.length}\n${json}`);
  }

  initSourceMaps(path: string) {
    path = this.qualifyPath(path);
    this._sourceMaps = new SourceMaps(dirname(path), this._workspaceDir);
  }

  public async run() {
    if (this._debug && this._stopOnEntry) {
      this.emit("stopOnEntry");
    } else {
      this.continue();
    }
  }

  public async continue() {
    if (this._state.state === 'running') {
      this.sendMessage({ type: 'continue' });
    } else {
      console.warn(`unexpected 'continue' call while in state ${this._state.state}`);
    }
  }

  public next(_granularity: "statement" | "line" | "instruction") {
    this.handleStep("next");
  }

  public stepIn(_targetId: number | undefined) {
    this.handleStep("stepIn");
  }

  public stepOut() {
    this.handleStep("stepOut");
  }

  private handleStep(type: "next" | "stepIn" | "stepOut") {
    this.sendMessage({ type });
  }

  public async stack(index: number, count: number): Promise<IRuntimeStack> {
    this.sendMessage({ type: "getStack", value: {
      index,
      count,
    }});

    const message = await this._stack.wait();
    
    const stack = message.value;
    for (let frame of stack) {
      if (frame.sourceLocation) {
        // Because JS is all by-reference, location objects can end up being
        // shared across frames or calls. We don't want qualification or translation
        // to be applied twice to the same location, so we always spin off a
        // new object instance to perform the translation on.
        let sourceLocation = { ...frame.sourceLocation };
        sourceLocation.path = this.qualifyPath(frame.sourceLocation.path);
        await this._translateLocationFromContent(sourceLocation);
        frame.sourceLocation = sourceLocation;
      }
    }
    return {
      count: stack.length,
      frames: stack,
    };
  }

  private async _translateLocationFromContent(loc: SourceLocation) {
    if (!this._sourceMaps) {
      return true;
    }
    let origColumn = loc.column;
    if (typeof loc.column === "number" && loc.column > 0) {
      loc.column -= 1;
    }
    let didMap = await this._sourceMaps.MapToSource(loc);
    if (!didMap) {
      loc.column = origColumn;  // revert the change we made for sourcemap processing
    }
    return didMap;
  }

  private async _translateLocationToContent(loc: SourceLocation) {
    if (!this._sourceMaps) {
      return true;
    }
    let origColumn = loc.column;
    if (typeof loc.column === "number") {
      loc.column += 1;
    }
    let didMap = await this._sourceMaps.MapFromSource(loc);
    if (!didMap) {
      loc.column = origColumn;  // revert the change we made for sourcemap processing
    }
    return didMap;
  }

  async getScopes(frameId: number): Promise<ReadonlyArray<Scope>> {
    this.sendMessage({ type: "getScopes", value: frameId });
    let message = await this._scopes.wait();
    return message.value;
  }

  public async getBreakpointLocations(
    path: string,
    line: number
  ): Promise<ReadonlyArray<{ line: number; column: number }>> {
    // TODO: support the full set of query params from BreakpointLocationsArguments
    path = this.normalizePath(path);

    let loc = new SourceLocation(path, line, 0);
    await this._translateLocationToContent(loc);
    
    await this.sendMessage({ type: "getBreakpointsForLine", value: loc });

    let message = await this._bpsForLine.wait();
    return message.value;
  }

  private _bpSet: Signal<BreakpointSetResponse, void> = new Signal();
  private _variables: Signal<VariablesResponse, void> = new Signal();
  private _eval: Signal<EvaluateResponse, void> = new Signal();
  private _variableSet: Signal<VariableSetResponse, void> = new Signal();
  private _stack: Signal<StackResponse, void> = new Signal();
  private _scopes: Signal<ScopesResponse, void> = new Signal();
  private _bpsForLine: Signal<BreakpointsForLineResponse, void> = new Signal();

  public async setBreakPoint(
    path: string,
    line: number,
    column?: number
  ): Promise<IRuntimeBreakpoint> {
    path = this.normalizePath(path);

    let loc = new SourceLocation(path, line, column ?? 0);
    await this._translateLocationToContent(loc);

    this.sendMessage({ type: "setBreakpoint", value: loc });

    // let response = await this.sendAndReceiveMessage({ type: "setBreakpoint", value: {
    //   path,
    //   line,
    //   column,
    // }});

    let response = await this._bpSet.wait();

    if (response.value.id !== -1) {
      loc.line = response.value.line;
      loc.column = response.value.column ?? 0;
    }
    await this._translateLocationFromContent(loc);

    return { id: response.value.id, ...loc };
    return response.value;
  }

  public async getVariables(reference: number): Promise<ReadonlyArray<IRuntimeVariable>> {
    this.sendMessage({ type: "getVariables", value: reference });
    let message = await this._variables.wait();
    return message.value;
  }

  public async setVariable(
    variablesReference: number,
    name: string,
    value: string
  ): Promise<IRuntimeVariable> {
    const jsValue: any = JSON.parse(value);
    // Manually encode the value so that it'll be decoded as raw values by the runtime, instead of everything becoming a string.
    // TODO: this seems extraordinarily illegal. What if value contains a double quote, etc. Or is it guaranteed not to by the debug protocol?
    // let rawValue = `{"variablesReference": ${variablesReference}, "name": "${name}", "value": ${value}}`;
    this.sendMessage(
      { type: "setVariable", value: {
        variablesReference,
        name,
        value: jsValue
      }}
    );

    let message = await this._variableSet.wait();
    return message.value;
  }

  public async evaluate(expression: string): Promise<{
    result: string;
    variablesReference: number;
  }> {
    this.sendMessage({ type: 'evaluate', value: { expression } });
    let message = await this._eval.wait();
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
    if (Path.isAbsolute(path)) {
      return path;
    }
    return `${this._workspaceDir}/${path}`;
  }
}
