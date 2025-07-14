import {
  Logger,
  logger,
  LoggingDebugSession,
  InitializedEvent,
  TerminatedEvent,
  StoppedEvent,
  OutputEvent,
  Thread,
  StackFrame,
  Source,
} from "@vscode/debugadapter";
import { DebugProtocol } from "@vscode/debugprotocol";
import { basename } from "path-browserify";
import {
  IStarlingMonkeyRuntimeConfig,
  StarlingMonkeyRuntime,
  FileAccessor,
} from "./starlingMonkeyRuntime.js";
import { failed } from "../shared/messages.js";

interface ILaunchRequestArguments extends DebugProtocol.LaunchRequestArguments {
  program: string;
  component: string;
  stopOnEntry?: boolean;
  trace?: boolean;
  noDebug?: boolean;
}

interface IAttachRequestArguments extends ILaunchRequestArguments {} // eslint-disable-line @typescript-eslint/no-empty-object-type

export class StarlingMonkeyDebugSession extends LoggingDebugSession {
  private static threadID = 1;
  private _runtime: StarlingMonkeyRuntime;
  private _initialized = false;

  public constructor(fileAccessor: FileAccessor, workspace: string, configuration: IStarlingMonkeyRuntimeConfig) {
    super();

    this.setDebuggerLinesStartAt1(true);
    this.setDebuggerColumnsStartAt1(true);

    this._runtime = new StarlingMonkeyRuntime(workspace, fileAccessor, configuration);

    // setup event handlers
    this._runtime.on("programLoaded", () => {
      this._initialized = true;
      this.sendEvent(new InitializedEvent());
    });
    this._runtime.on("stopOnEntry", () => {
      this.sendEvent(
        new StoppedEvent("entry", StarlingMonkeyDebugSession.threadID)
      );
    });
    this._runtime.on("stopOnStep", () => {
      this.sendEvent(
        new StoppedEvent("step", StarlingMonkeyDebugSession.threadID)
      );
    });
    this._runtime.on("stopOnBreakpoint", () => {
      this.sendEvent(
        new StoppedEvent("breakpoint", StarlingMonkeyDebugSession.threadID)
      );
    });
    this._runtime.on("stopOnDataBreakpoint", () => {
      this.sendEvent(
        new StoppedEvent("data breakpoint", StarlingMonkeyDebugSession.threadID)
      );
    });
    this._runtime.on("stopOnInstructionBreakpoint", () => {
      this.sendEvent(
        new StoppedEvent(
          "instruction breakpoint",
          StarlingMonkeyDebugSession.threadID
        )
      );
    });
    this._runtime.on("stopOnException", (exception) => {
      if (exception) {
        this.sendEvent(
          new StoppedEvent(
            `exception(${exception})`,
            StarlingMonkeyDebugSession.threadID
          )
        );
      } else {
        this.sendEvent(
          new StoppedEvent("exception", StarlingMonkeyDebugSession.threadID)
        );
      }
    });
    this._runtime.on("output", (type, text, filePath, line, column) => {
      let category: string;
      switch (type) {
        case "prio":
          category = "important";
          break;
        case "out":
          category = "stdout";
          break;
        case "err":
          category = "stderr";
          break;
        default:
          category = "console";
          break;
      }
      const e: DebugProtocol.OutputEvent = new OutputEvent(
        `${text}\n`,
        category
      );

      if (text === "start" || text === "startCollapsed" || text === "end") {
        e.body.group = text;
        e.body.output = `group-${text}\n`;
      }

      e.body.source = this.createSource(filePath);
      e.body.line = this.convertDebuggerLineToClient(line);
      e.body.column = this.convertDebuggerColumnToClient(column);
      this.sendEvent(e);
    });
    this._runtime.on("end", () => {
      this.sendEvent(new TerminatedEvent());
    });
  }

  protected initializeRequest(
    response: DebugProtocol.InitializeResponse,
    _args: DebugProtocol.InitializeRequestArguments
  ): void {
    response.body = {
      ...response.body,
      supportsSetVariable: true,
      supportsConfigurationDoneRequest: true,
      supportsBreakpointLocationsRequest: true,
    };

    this.sendResponse(response);
  }

  protected configurationDoneRequest(
    response: DebugProtocol.ConfigurationDoneResponse,
    args: DebugProtocol.ConfigurationDoneArguments
  ): void {
    super.configurationDoneRequest(response, args);
    this._runtime.run();
  }

  protected disconnectRequest(
    _response: DebugProtocol.DisconnectResponse,
    args: DebugProtocol.DisconnectArguments,
    _request?: DebugProtocol.Request
  ): void {
    console.log(
      `disconnectRequest suspend: ${args.suspendDebuggee}, terminate: ${args.terminateDebuggee}`
    );
  }

  protected async attachRequest(
    response: DebugProtocol.AttachResponse,
    args: IAttachRequestArguments
  ) {
    return this.launchRequest(response, args);
  }

  protected async launchRequest(
    response: DebugProtocol.LaunchResponse,
    args: ILaunchRequestArguments
  ) {
    logger.setup(
      args.trace ? Logger.LogLevel.Verbose : Logger.LogLevel.Stop,
      false
    );
    this._runtime.start(args.program, args.component, !!args.stopOnEntry, !args.noDebug);
    this.sendResponse(response);
  }

  protected setFunctionBreakPointsRequest(
    response: DebugProtocol.SetFunctionBreakpointsResponse,
    _args: DebugProtocol.SetFunctionBreakpointsArguments,
    _request?: DebugProtocol.Request
  ): void {
    this.sendResponse(response);
  }

  protected async setBreakPointsRequest(
    response: DebugProtocol.SetBreakpointsResponse,
    args: DebugProtocol.SetBreakpointsArguments
  ): Promise<void> {
    const path = args.source.path as string;
    response.body = { breakpoints: [] };
    const breakpoints = response.body.breakpoints;
    if (args.breakpoints) {
      for (const bp of args.breakpoints) {
        const line = this.convertClientLineToDebugger(bp.line);
        const column = bp.column
          ? this.convertClientColumnToDebugger(bp.column)
          : undefined;
        const result = await this._runtime.setBreakPoint(path, line, column);
        if (result.id === -1) {
          breakpoints.push({
            verified: false,
            line: bp.line,
            column: bp.column,
            message: "Failed to set breakpoint",
          });
        } else {
          breakpoints.push({
            verified: true,
            id: result.id,
            line: result.line,
            column: result.column,
          });
        }
      }
    }
    this.sendResponse(response);
  }

  protected async breakpointLocationsRequest(
    response: DebugProtocol.BreakpointLocationsResponse,
    args: DebugProtocol.BreakpointLocationsArguments,
    _request?: DebugProtocol.Request
  ): Promise<void> {
    // VSCode apparently sends this request before the `Initialized` event is sent.
    // In that case, we can't answer it yet and instead ignore it.
    if (!this._initialized) {
      return;
    }
    if (args.source.path) {
      const bps = await this._runtime.getBreakpointLocations(
        args.source.path,
        this.convertClientLineToDebugger(args.line)
      );
      response.body = {
        breakpoints: bps.map(({ line, column }) => {
          return {
            line: this.convertDebuggerColumnToClient(line),
            column: this.convertDebuggerColumnToClient(column),
          };
        }),
      };
    } else {
      response.body = {
        breakpoints: [],
      };
    }
    this.sendResponse(response);
  }

  protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
    response.body = {
      threads: [new Thread(StarlingMonkeyDebugSession.threadID, "Main")],
    };
    this.sendResponse(response);
  }

  protected stackTraceRequest(
    response: DebugProtocol.StackTraceResponse,
    args: DebugProtocol.StackTraceArguments
  ): void {
    const startFrame = args.startFrame ?? 0;
    const maxLevels = args.levels || 1000;

    this._runtime.stack(startFrame, maxLevels).then((stk) => {
      response.body = {
        stackFrames: stk.frames.map((f) => {
          const sf: DebugProtocol.StackFrame = new StackFrame(
            f.index,
            f.name,
            this.createSource(f.sourceLocation?.path),
          );
          if (f.sourceLocation) {
            sf.line = this.convertDebuggerLineToClient(f.sourceLocation.line);
            sf.column = this.convertDebuggerColumnToClient(f.sourceLocation.column);
          }

          return sf;
        }),
        // 4 options for 'totalFrames':
        //omit totalFrames property: 	// VS Code has to probe/guess. Should result in a max. of two requests
        totalFrames: stk.count, // stk.count is the correct size, should result in a max. of two requests
        //totalFrames: 1000000 			// not the correct size, should result in a max. of two requests
        //totalFrames: endFrame + 20 	// dynamically increases the size with every requested chunk, results in paging
      };
      this.sendResponse(response);
    });
  }

  protected async scopesRequest(
    response: DebugProtocol.ScopesResponse,
    args: DebugProtocol.ScopesArguments
  ): Promise<void> {
    const scopes = await this._runtime.getScopes(args.frameId);
    response.body = { scopes: scopes.slice() };
    this.sendResponse(response);
  }

  protected async variablesRequest(
    response: DebugProtocol.VariablesResponse,
    args: DebugProtocol.VariablesArguments,
    _request?: DebugProtocol.Request
  ): Promise<void> {
    const variables = await this._runtime.getVariables(args.variablesReference);
    response.body = { variables: variables.slice() };
    this.sendResponse(response);
  }

  protected async setVariableRequest(
    response: DebugProtocol.SetVariableResponse,
    args: DebugProtocol.SetVariableArguments
  ): Promise<void> {
    // TODO: implement
    const message = await this._runtime.setVariable(
      args.variablesReference,
      args.name,
      args.value
    );
    if (failed(message)) {
      response.success = false;
      response.message = message.error;
    } else {
      response.body = { value: message.value, type: message.type, variablesReference: message.variablesReference };
    }
    this.sendResponse(response);
  }

  protected async evaluateRequest(
    response: DebugProtocol.EvaluateResponse,
    args: DebugProtocol.EvaluateArguments,
    _request?: DebugProtocol.Request
  ): Promise<void> {
    const result = await this._runtime.evaluate(args.expression);
    response.body = result;
    this.sendResponse(response);
  }

  protected continueRequest(
    response: DebugProtocol.ContinueResponse,
    _args: DebugProtocol.ContinueArguments
  ): void {
    this.sendResponse(response);
    this._runtime.continue();
  }

  protected nextRequest(
    response: DebugProtocol.NextResponse,
    args: DebugProtocol.NextArguments
  ): void {
    this.sendResponse(response);
    this._runtime.next(args.granularity ?? "statement");
  }

  protected stepInRequest(
    response: DebugProtocol.StepInResponse,
    args: DebugProtocol.StepInArguments
  ): void {
    this._runtime.stepIn(args.targetId);
    this.sendResponse(response);
  }

  protected stepOutRequest(
    response: DebugProtocol.StepOutResponse,
    _args: DebugProtocol.StepOutArguments
  ): void {
    this._runtime.stepOut();
    this.sendResponse(response);
  }

  private createSource(filePath: string | undefined): Source | undefined {
    if (!filePath) {
      return undefined;
    }

    return new Source(
      basename(filePath),
      this.convertDebuggerPathToClient(filePath),
      undefined,
      undefined,
      "starlingmonkey-adapter-data"
    );
  }
}
