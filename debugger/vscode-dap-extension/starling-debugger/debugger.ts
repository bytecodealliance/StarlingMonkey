// Type definitions for SpiderMonkey Debugger API
declare class Debugger {
  static Object: any;
  static Script: any;
  static Environment: any;
  static Frame: any;
  constructor();
  addAllGlobalsAsDebuggees(): void;
  findScripts(): Debugger.Script[];
  onNewScript: ((script: Debugger.Script, global?: any) => void) | undefined;
  onEnterFrame: ((frame: Debugger.Frame) => void) | undefined;
}

declare namespace Debugger {
  interface Script {
    url: string;
    startLine: number;
    startColumn: number;
    lineCount: number;
    global: Object;
    getOffsetMetadata(offset: number): { lineNumber: number; columnNumber: number };
    getPossibleBreakpointOffsets(options: { line: number }): number[];
    getChildScripts(): Script[];
    setBreakpoint(offset: number, handler: BreakpointHandler): void;
  }

  interface Frame {
    script: Script;
    offset: number;
    this?: Object;
    type: string;
    older?: Frame;
    olderSavedFrame?: Frame;
    callee?: { name: string };
    environment: Environment;
    onStep: (() => void) | undefined;
    onPop: (() => void) | undefined;
  }

  interface Environment {
    names(): string[];
    getVariable(name: string): any;
    setVariable(name: string, value: any): void;
  }

  interface Object {
    class?: string;
    getOwnPropertyNames(): string[];
    getOwnPropertyDescriptor(name: string): PropertyDescriptor;
    setProperty(name: string, value: any): void;
  }

  interface PropertyDescriptor {
    value?: any;
    get?: Object;
    set?: Object;
  }

  interface BreakpointHandler {
    hit(frame: Frame): void;
  }
}

declare const socket: {
  send(data: string): void;
  receive(bytes: number): string;
};

declare function print(message: string): void;
declare function assert(condition: any, message?: string): asserts condition;
declare function setContentPath(path: string): void;

let LOG = false;

try {
  let dbg = new Debugger();
  dbg.addAllGlobalsAsDebuggees();

  let scripts = new Map<string, Set<Debugger.Script>>();
  let currentFrame: Debugger.Frame | undefined;
  let lastLine = 0;
  let lastColumn = 0;
  // Reserve the first 0xFFF for stack frames.
  const MAX_FRAMES = 0xfff;
  const GLOBAL_OBJECT_REF = MAX_FRAMES + 1;
  const OBJECT_REFS_START = GLOBAL_OBJECT_REF + 1;
  let varRefsIndex = OBJECT_REFS_START;
  let objectToId = new Map<Debugger.Object, number>();
  let idToObject = new Map<number, Debugger.Object>();

  function addScript(script: Debugger.Script): void {
    let urlScripts = scripts.get(script.url);
    if (!urlScripts) {
      urlScripts = new Set();
      scripts.set(script.url, urlScripts);
    }
    urlScripts.add(script);
  }

  dbg.onNewScript = function (script: Debugger.Script, _global?: any): void {
    if (scripts.has(script.url)) {
      LOG && print(`Warning: script with url ${script.url} already loaded`);
    }
    addScript(script);
  };

  dbg.onEnterFrame = function (frame: Debugger.Frame): void {
    dbg.onEnterFrame = undefined;
    for (let script of dbg.findScripts()) {
      addScript(script);
    }
    sendMessage("programLoaded");
    return handlePausedFrame(frame);
  };

  function handlePausedFrame(frame: Debugger.Frame): void {
    try {
      dbg.onEnterFrame = undefined;
      if (currentFrame) {
        currentFrame.onStep = undefined;
        currentFrame.onPop = undefined;
      }
      currentFrame = frame;
      idToObject.set(GLOBAL_OBJECT_REF, frame.script.global);
      objectToId.set(frame.script.global, GLOBAL_OBJECT_REF);
      varRefsIndex = OBJECT_REFS_START;
      setCurrentPosition();
      waitForSocket();
      objectToId.clear();
      idToObject.clear();
    } catch (e) {
      assert(e instanceof Error);
      LOG && print(
          `Exception during paused frame handling: ${e}. Stack:\n${e.stack}`
        );
    }
  }

  interface Message {
    type: string;
    value?: any;
  }

  function waitForSocket(): void {
    while (true) {
      try {
        let message = receiveMessage();
        LOG && print(`received message ${JSON.stringify(message)}`);
        switch (message.type) {
          case "loadProgram":
            setContentPath(message.value);
            return;
          case "getBreakpointsForLine":
            getBreakpointsForLine(message.value);
            break;
          case "setBreakpoint":
            setBreakpoint(message.value);
            break;
          case "getStack":
            getStack(message.value.index, message.value.count);
            break;
          case "getScopes":
            getScopes(message.value);
            break;
          case "getVariables":
            getVariables(message.value);
            break;
          case "setVariable":
            setVariable(message.value);
            break;
          case "next":
            currentFrame!.onStep = handleNext;
            return;
          case "stepIn":
            currentFrame!.onStep = handleNext;
            dbg.onEnterFrame = handleStepIn;
            return;
          case "stepOut":
            currentFrame!.onPop = handleStepOut;
            return;
          case "continue":
            currentFrame = undefined;
            return;
          case "startDebugLogging":
            LOG = true;
            break;
          case "stopDebugLogging":
            LOG = false;
            break;
          default:
            LOG && print(
                `Invalid message received, continuing execution. Message: ${message.type}`
              );
            currentFrame = undefined;
            return;
        }
      } catch (e) {
        assert(e instanceof Error);
        LOG && print(`Exception during paused frame loop: ${e}. Stack:\n${e.stack}`);
      }
    }
  }

  function setCurrentPosition(): void {
    if (!currentFrame) {
      lastLine = 0;
      lastColumn = 0;
      return;
    }
    let offsetMeta = currentFrame.script.getOffsetMetadata(currentFrame.offset);
    lastLine = offsetMeta.lineNumber;
    lastColumn = offsetMeta.columnNumber;
  }

  function positionChanged(frame: Debugger.Frame): boolean {
    let offsetMeta = frame.script.getOffsetMetadata(frame.offset);
    return (
      offsetMeta.lineNumber !== lastLine ||
      offsetMeta.columnNumber !== lastColumn
    );
  }

  function handleNext(this: Debugger.Frame): void {
    if (!positionChanged(this)) {
      return;
    }
    sendMessage("stopOnStep");
    handlePausedFrame(this);
  }

  function handleStepIn(frame: Debugger.Frame): void {
    dbg.onEnterFrame = undefined;
    sendMessage("stopOnStep");
    handlePausedFrame(frame);
  }

  function handleStepOut(this: Debugger.Frame): void {
    this.onPop = undefined;
    if (this.older) {
      this.older.onStep = handleNext;
    } else {
      dbg.onEnterFrame = handleStepIn;
    }
  }

  const breakpointHandler: Debugger.BreakpointHandler = {
    hit(frame: Debugger.Frame): void {
      sendMessage("breakpointHit", frame.offset);
      return handlePausedFrame(frame);
    },
  };

  interface BreakpointLocation {
    script: Debugger.Script;
    offsets: number[];
  }

  function getPossibleBreakpointsInScripts(
    scripts: Set<Debugger.Script> | undefined,
    line: number
  ): BreakpointLocation | null {
    if (!scripts) {
      return null;
    }
    for (let script of scripts) {
      let result = getPossibleBreakpointsInScriptRecursive(script, line);
      if (result) {
        return result;
      }
    }
    return null;
  }

  function getPossibleBreakpointsInScriptRecursive(
    script: Debugger.Script,
    line: number
  ): BreakpointLocation | null {
    let offsets = script.getPossibleBreakpointOffsets({ line });
    if (offsets.length) {
      return { script, offsets };
    }

    for (let child of script.getChildScripts()) {
      let result = getPossibleBreakpointsInScriptRecursive(child, line);
      if (result) {
        return result;
      }
    }
    return null;
  }

  function getBreakpointsForLine({
    path,
    line,
  }: {
    path: string;
    line: number;
  }): void {
    let fileScripts = scripts.get(path);
    let { script, offsets } =
      getPossibleBreakpointsInScripts(fileScripts, line) || {};
    let locations: { line: number; column: number }[] = [];
    if (offsets) {
      locations = offsets.map((offset) => {
        let meta = script!.getOffsetMetadata(offset);
        return {
          line: meta.lineNumber,
          column: meta.columnNumber,
        };
      });
    }
    sendMessage("breakpointsForLine", locations);
  }

  function setBreakpoint({
    path,
    line,
    column,
  }: {
    path: string;
    line: number;
    column: number;
  }): void {
    let fileScripts = scripts.get(path);
    if (!fileScripts) {
      LOG && print(`Can't set breakpoint: no scripts found for file ${path}`);
      sendMessage("breakpointSet", { id: -1, line, column });
      return;
    }
    let { script, offsets } =
      getPossibleBreakpointsInScripts(fileScripts, line) || {};
    let offset = -1;
    if (offsets) {
      for (offset of offsets) {
        let meta = script!.getOffsetMetadata(offset);
        assert(
          meta.lineNumber === line,
          `Line number mismatch, should be ${line}, got ${meta.lineNumber}`
        );
        if (meta.columnNumber === column) {
          break;
        }
      }
      script!.setBreakpoint(offset, breakpointHandler);
    }
    sendMessage("breakpointSet", { id: offset, line, column });
  }

  interface IRuntimeStackFrame {
    index: number;
    name?: string;
    file?: string;
    line?: number;
    column?: number;
    instruction?: number;
  }
  function getStack(index: number, count: number): void {
    let stack: IRuntimeStackFrame[] = [];
    assert(currentFrame);
    let frame = findFrame(currentFrame, index);

    while (stack.length < count) {
      let entry: IRuntimeStackFrame = {
        index: stack.length,
      };
      if (frame.script) {
        const offsetMeta = frame.script.getOffsetMetadata(frame.offset);
        entry.file = frame.script.url;
        entry.line = offsetMeta.lineNumber;
        entry.column = offsetMeta.columnNumber;
      }

      if (frame.callee) {
        entry.name = frame.callee.name;
      } else {
        entry.name = frame.type;
      }
      stack.push(entry);
      let nextFrame = frame.older || frame.olderSavedFrame;
      if (!nextFrame) {
        break;
      }
      frame = nextFrame;
    }
    sendMessage("stack", stack);
  }

  interface Scope {
    name: string;
    presentationHint?: 'arguments' | 'locals' | 'registers' | string;
    variablesReference: number;
    namedVariables?: number;
    indexedVariables?: number;
    expensive: boolean;
    // source?: Source; TODO: support
    line?: number;
    column?: number;
    endLine?: number;
    endColumn?: number;
  }

  function getScopes(index: number): void {
    assert(currentFrame);
    let frame = findFrame(currentFrame, index);
    let script = frame.script;
    let scopes: Scope[] = [{
      name: "Locals",
      presentationHint: "locals",
      variablesReference: index + 1,
      expensive: false,
      line: script.startLine,
      column: script.startColumn,
      endLine: script.startLine + script.lineCount,
    },
    {
      name: "Globals",
      presentationHint: "globals",
      variablesReference: GLOBAL_OBJECT_REF,
      expensive: true,
    }];

    sendMessage("scopes", scopes);
  }

  function getVariables(reference: number): void {
    if (reference > MAX_FRAMES) {
      let object = idToObject.get(reference);
      let locals = getMembers(object!);
      sendMessage("variables", locals);
      return;
    }

    assert(currentFrame);
    let frame = findFrame(currentFrame, reference - 1);
    let locals = [];

    for (let name of frame.environment.names()) {
      let value = frame.environment.getVariable(name);
      locals.push({ name, ...formatValue(value) });
    }

    if (frame.this) {
      let { value, type, variablesReference } = formatValue(frame.this);
      locals.push({
        name: "<this>",
        value,
        type,
        variablesReference,
      });
    }

    sendMessage("variables", locals);
  }

  function setVariable({
    variablesReference,
    name,
    value,
  }: {
    variablesReference: number;
    name: string;
    value: any;
  }): void {
    let newValue;
    if (variablesReference > MAX_FRAMES) {
      let object = idToObject.get(variablesReference);
      assert(object);
      object.setProperty(name, value);
      newValue = getMember(object, name);
    } else {
      assert(currentFrame);
      let frame = findFrame(currentFrame, variablesReference - 1);
      frame.environment.setVariable(name, value);
      newValue = formatValue(frame.environment.getVariable(name));
    }
    sendMessage("variableSet", newValue);
  }

  function getMembers(object: Debugger.Object): any[] {
    let names = object.getOwnPropertyNames();
    let members = [];
    for (let name of names) {
      members.push(getMember(object, name));
    }
    return members;
  }

  function getMember(object: Debugger.Object, name: string): any {
    let descriptor = object.getOwnPropertyDescriptor(name);
    return { name, ...formatDescriptor(descriptor) };
  }

  function formatValue(value: any): {
    value: string;
    type: string;
    variablesReference: number;
  } {
    let formatted;
    let type: string = typeof value;
    let structured = false;
    type = type[0].toUpperCase() + type.slice(1);
    if (type === "Object") {
      if (value === null) {
        formatted = "null";
        type = "Null";
      } else if (!(value instanceof Debugger.Object) && value.uninitialized) {
        formatted = "<uninitialized>";
        type = "Uninitialized Binding";
      } else {
        type = value.class ?? "Object";
        formatted = `[object ${type}]`;
        structured = true;
      }
    } else if (type === "String") {
      formatted = `"${value}"`;
    } else {
      formatted = `${value}`;
    }
    let variablesReference = 0;
    if (structured) {
      if (!objectToId.has(value)) {
        variablesReference = varRefsIndex++;
        idToObject.set(variablesReference, value);
        objectToId.set(value, variablesReference);
      }
    }
    return { value: formatted, type, variablesReference };
  }

  function formatDescriptor(descriptor: Debugger.PropertyDescriptor): {
    value: string;
    type: string;
    variablesReference: number;
  } {
    if (descriptor.value) {
      return formatValue(descriptor.value);
    }

    let formatted;
    if (descriptor.get) {
      formatted = formatValue(descriptor.get);
    }

    if (descriptor.set) {
      let setter = formatValue(descriptor.set);
      if (formatted) {
        formatted += `, ${setter}`;
      } else {
        formatted = setter;
      }
    }

    return { value: formatted, type: "Accessor", variablesReference: 0 };
  }

  function findFrame(
    start: Debugger.Frame,
    index: number
  ): Debugger.Frame {
    let frame = start;
    for (let i = 0; i < index && frame; i++) {
      let nextFrame = frame.older || frame.olderSavedFrame;
      frame = nextFrame!;
      assert(frame, `Frame with index ${index} not found`);
    }
    return frame;
  }

  function sendMessage(type, value?) {
    const messageStr = JSON.stringify({ type, value });
    LOG && print(`sending message: ${messageStr}`);
    socket.send(`${messageStr.length}\n${messageStr}`);
  }

  function receiveMessage(): Message {
    LOG && print("Debugger listening for incoming message ...");
    let partialMessage = "";
    let eol = -1;
    while (true) {
      partialMessage += socket.receive(10);
      eol = partialMessage.indexOf("\n");
      if (eol >= 0) {
        break;
      }
    }

    let length = parseInt(partialMessage.slice(0, eol), 10);
    if (isNaN(length)) {
      LOG && print(
          `WARN: Received message ${partialMessage} not of the format '[length]\\n[JSON encoded message with length {length}]', discarding`
        );
      return receiveMessage();
    }
    partialMessage = partialMessage.slice(eol + 1);

    while (partialMessage.length < length) {
      partialMessage += socket.receive(length - partialMessage.length);
    }

    if (partialMessage.length > length) {
      LOG && print(
          `WARN: Received message ${
            partialMessage.length - length
          } bytes longer than advertised, ignoring everything beyond the first ${length} bytes`
        );
      partialMessage = partialMessage.slice(0, length);
    }

    try {
      return JSON.parse(partialMessage);
    } catch (e) {
      assert(e instanceof Error);
      LOG && print(`WARN: Ill-formed message received, discarding: ${e}, ${e.stack}`);
      return receiveMessage();
    }
  }

  sendMessage("connect");
  waitForSocket();
} catch (e) {
  assert(e instanceof Error);
  LOG && print(`Setting up connection to debugger failed with exception: ${e},\nstack:\n${e.stack}`);
}
