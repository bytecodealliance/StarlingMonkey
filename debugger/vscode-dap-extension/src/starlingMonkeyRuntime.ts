import { Scope } from "@vscode/debugadapter";
import { EventEmitter } from "events";
import * as Net from "net";
import { Signal } from "./signals.js";
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
  file?: string;
  line?: number;
  column?: number;
  instruction?: number;
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

// Messages sent from the SMRuntime to the ComponentResourceInstance.
// These are sent as JSON via the SMR _server socket (and then seem
// to be passed on to the debugger.ts script).
//
// These appear to be sent but it's not clear if all of them
// are processed by the CRI
type RuntimeToInstanceMessage =
  | LoadProgramMessage
  | ContinueMessage
  | GetStackMessage
  | GetScopesMessage
  | GetBreakpointsForLineMessage
  | SetBreakpointMessage
  | GetVariablesMessage
  | SetVariableMessage
  | EvaluateMessage
  | StartDebugLoggingMessage
  | StopDebugLoggingMessage;

interface LoadProgramMessage {
  type: 'loadProgram';
  value: string; // source file
}

interface ContinueMessage {
  type: 'continue' | 'next' | 'stepIn' | 'stepOut';
  value?: undefined,
}

interface GetStackMessage {
  type: 'getStack';
  value: {
    index: number;
    count: number;
  }
}

interface GetScopesMessage {
  type: 'getScopes';
  value: number; // frameId
}

interface GetBreakpointsForLineMessage {
  type: 'getBreakpointsForLine';
  value: {
    path: string;
    line: number;
  }
}

interface SetBreakpointMessage {
  type: 'setBreakpoint';
  value: {
    path: string;
    line: number;
    column?: number;
  }
}

interface GetVariablesMessage {
  type: 'getVariables';
  value: number; // reference
}

interface SetVariableMessage {
  type: 'setVariable';
  value: string; // manually encoded JSON text
}
interface EvaluateMessage {
  type: 'evaluate';
  value: {
    expression: string;
  }
}
interface StartDebugLoggingMessage {
  type: 'startDebugLogging';
  value?: undefined;
}

interface StopDebugLoggingMessage {
  type: 'stopDebugLogging';
  value?: undefined;
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

// Messages from the ComponentRuntimeInstance to the SMRuntime,
// received as JSON via SMRuntime._server socket.
export type InstanceToRuntimeMessage =
  | IConnectMessage
  | IProgramLoadedMessage
  | IBreakpointHitMessage
  | IStopOnStepMessage
  | StackResponse
  | ScopesResponse
  | BreakpointsForLineResponse
  | BreakpointSetResponse
  | VariablesResponse
  | VariableSetResponse
  | EvaluateResponse;

interface IConnectMessage {
  type: 'connect';
}
interface IProgramLoadedMessage {
  type: 'programLoaded';
}
interface IBreakpointHitMessage {
  type: 'breakpointHit';
  value: number; // offset into frame
}
interface IStopOnStepMessage {
  type: 'stopOnStep';
}
interface StackResponse {
  type: 'stack';
  value: ReadonlyArray<IRuntimeStackFrame>;
}
interface ScopesResponse {
  type: 'scopes';
  value: ReadonlyArray<Scope>,
}
interface BreakpointsForLineResponse {
  type: 'breakpointsForLine';
  value: ReadonlyArray<{
    line: number;
    column: number,
  }>
}
interface BreakpointSetResponse {
  type: 'breakpointSet';
  value: IRuntimeBreakpoint;
}
interface VariablesResponse {
  type: 'variables',
  value: ReadonlyArray<IRuntimeVariable>,
  diagnostics: string;
}
interface VariableSetResponse {
  type: 'variableSet',
  value: IRuntimeVariable,
}
interface EvaluateResponse {
  type: 'evaluate';
  value: {
    result: string;
    variablesReference: number;
  }
}

function assert(condition: any, msg?: string): asserts condition {
  console.assert(condition, msg);
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

export class StarlingMonkeyRuntime extends EventEmitter<RuntimeEventMap> {
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
        console.warn(`*** we have some variables! ${message.value.map(v => v.name)}`);
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

      // console.debug(`*** SUBSEQUENT FROM WHEREVER. data=${data}`);
      console.debug(`<-- received: ${data}`);

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
      console.debug(`  <-- parsed: ${message}`);
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

  private sendMessage(message: RuntimeToInstanceMessage, useRawValue = false) {
    let json: string;
    if (useRawValue) {
      json = `{"type": "${message.type}", "value": ${message.value}}`;
    } else {
      json = JSON.stringify(message);
    }
    // console.debug(`sending message to runtime: ${message}`);
    this._socket.write(`${json.length}\n${json}`);
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

    // this.sendMessage({ type: 'continue' });
    // let message = await this.sendAndReceiveMessage({ type: "continue" });
    // // TODO: handle other results, such as run to completion
    // assert(
    //   message.type === "breakpointHit",
    //   `expected "breakpointHit" message, got "${message.type}"`
    // );
    // this.emit("stopOnBreakpoint");
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
    // let message = await this.sendAndReceiveMessage({ type });
    // // TODO: handle other results, such as run to completion
    // // TODO: this can barf if the step lands you on a breakpoint
    // assert(
    //   message.type === "stopOnStep",
    //   `expected "stopOnStep" message, got "${message.type}"`
    // );
    // this.emit("stopOnStep");
  }

  public async stack(index: number, count: number): Promise<IRuntimeStack> {
    this.sendMessage({ type: "getStack", value: {
      index,
      count,
    }});

    // let message = await this.sendAndReceiveMessage({ type: "getStack", value: {
    //   index,
    //   count,
    // }});
    // assert(
    //   message.type === "stack",
    //   `expected "stack" message, got "${message.type}"`
    // );

    let message = await this._stack.wait();
    
    let stack = message.value;
    for (let frame of stack) {
      if (frame.file) {
        frame.file = this.qualifyPath(frame.file);
      }
    }
    return {
      count: stack.length,
      frames: stack,
    };
  }

  async getScopes(frameId: number): Promise<ReadonlyArray<Scope>> {
    // let message = await this.sendAndReceiveMessage({ type: "getScopes", value: frameId });
    this.sendMessage({ type: "getScopes", value: frameId });
    let message = await this._scopes.wait();
    // assert(
    //   message.type === "scopes",
    //   `expected "scopes" message, got "${message.type}"`
    // );
    return message.value;
  }

  public async getBreakpointLocations(
    path: string,
    line: number
  ): Promise<ReadonlyArray<{ line: number; column: number }>> {
    // TODO: support the full set of query params from BreakpointLocationsArguments
    path = this.normalizePath(path);
    // let message = await this.sendAndReceiveMessage({ type: "getBreakpointsForLine", value: {
    //   path,
    //   line,
    // }});
    await this.sendMessage({ type: "getBreakpointsForLine", value: {
      path,
      line,
    }});

    let message = await this._bpsForLine.wait();
    // assert(
    //   message.type === "breakpointsForLine",
    //   `expected "breakpointsForLine" message, got "${message.type}"`
    // );
    console.debug(`returning BPs, message.value=${message.value}`);
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

    this.sendMessage({ type: "setBreakpoint", value: {
      path,
      line,
      column,
    }});

    // let response = await this.sendAndReceiveMessage({ type: "setBreakpoint", value: {
    //   path,
    //   line,
    //   column,
    // }});

    let response = await this._bpSet.wait();

    return response.value;
  }

  public async getVariables(reference: number): Promise<ReadonlyArray<IRuntimeVariable>> {
    console.warn(`*** asking for variable #${reference}`);
    this.sendMessage({ type: "getVariables", value: reference });
    // let message = await this.sendAndReceiveMessage({ type: "getVariables", value: reference });
    let message = await this._variables.wait();
    console.warn(`*** ...and got response for variable #${reference}: DIAG=${message.diagnostics}`);
    // assert(
    //   message.type === "variables",
    //   `expected "variables" message, got "${message.type}"`
    // );
    return message.value;
  }

  public async setVariable(
    variablesReference: number,
    name: string,
    value: string
  ): Promise<IRuntimeVariable> {
    // Manually encode the value so that it'll be decoded as raw values by the runtime, instead of everything becoming a string.
    // TODO: this seems extraordinarily illegal. What if value contains a double quote, etc. Or is it guaranteed not to by the debug protocol?
    let rawValue = `{"variablesReference": ${variablesReference}, "name": "${name}", "value": ${value}}`;
    // let message = await this.sendAndReceiveMessage(
    //   { type: "setVariable", value: rawValue },
    //   true
    // );
    this.sendMessage(
      { type: "setVariable", value: rawValue },
      true
    );

    let message = await this._variableSet.wait();

    // assert(
    //   message.type === "variableSet",
    //   `expected "variableSet" message, got "${message.type}"`
    // );
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
    return `${this._workspaceDir}/${path}`;
  }
}
