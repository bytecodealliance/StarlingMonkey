import type { SourceLocation } from "../src/sourcemaps/sourceMaps";
import type { Scope } from "@vscode/debugadapter";

export interface IRuntimeBreakpoint {
  id: number;
  line: number;
  column: number;
}

export interface IRuntimeStackFrame {
  index: number;
  name: string;
  // Passing the path-line-column info as an object instead of as three
  // fields can cause surprises with sharing (see the `stack` method). We
  // do it anyway because the three fields are included or omitted together,
  // and TS has no way of expressing that other than making them an object.
  sourceLocation?: SourceLocation;
  instruction?: number;
}

// Messages sent from the SMRuntime to the ComponentResourceInstance.
// These are sent as JSON via the SMR _server socket (and then seem
// to be passed on to the debugger.ts script).
//
// These appear to be sent but it's not clear if all of them
// are processed by the CRI
export type RuntimeToInstanceMessage =
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
    column: number;
  }
}

interface GetVariablesMessage {
  type: 'getVariables';
  value: number; // reference
}

interface SetVariableMessage {
  type: 'setVariable';
  value: {
    variablesReference: number;
    name: string;
    value: unknown;
  }
}
interface EvaluateMessage {
  type: 'evaluate';
  value: {
    expression: string;
  }
}
interface StartDebugLoggingMessage {
  type: 'startDebugLogging';
}

interface StopDebugLoggingMessage {
  type: 'stopDebugLogging';
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
  value: string; // path to loaded program
}
interface IBreakpointHitMessage {
  type: 'breakpointHit';
  value: number; // offset into frame
}
interface IStopOnStepMessage {
  type: 'stopOnStep';
}
export interface StackResponse {
  type: 'stack';
  value: ReadonlyArray<IRuntimeStackFrame>;
}
export interface ScopesResponse {
  type: 'scopes';
  value: ReadonlyArray<Scope>,
}
export interface BreakpointsForLineResponse {
  type: 'breakpointsForLine';
  value: ReadonlyArray<{
    line: number;
    column: number,
  }>
}
export interface BreakpointSetResponse {
  type: 'breakpointSet';
  value: IRuntimeBreakpoint;
}
export interface VariablesResponse {
  type: 'variables',
  value: ReadonlyArray<IRuntimeVariable>,
}
export type VariableSetResponse = TypedErrorable<'variableSet', IRuntimeVariable>;
// export interface VariableSetResponse {
//   type: 'variableSet',
//   value: IRuntimeVariable,
// }
// export interface VariableSetErrorResponse {
//   type: 'variableSet',
//   error: string,
// }
export interface EvaluateResponse {
  type: 'evaluate';
  value: {
    result: string;
    variablesReference: number;
  }
}
export interface IRuntimeVariable {
  name: string;
  value: string;
  type: string;
  variablesReference: number;
}

export function succeeded<M, V>(message: TypedErrorable<M, V>): message is { type: M, value: V } {
  return !failed(message);
}
export function failed<T>(e: Errorable<T>): e is { error: string } {
  return (<{ error?: string }>e).error !== undefined;
}

export type TypedErrorable<M, V> = { type: M, value: V } | { type: M, error: string };
export type Errorable<V> = V | { error: string };
