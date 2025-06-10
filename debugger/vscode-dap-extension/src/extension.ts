import {
  ExtensionContext,
} from "vscode";
import {
  activateStarlingMonkeyDebug,
} from "./activateStarlingMonkeyDebugger.js";

const runMode = "inline" as const;  // TODO: bring back 'server' option

export function activate(context: ExtensionContext) {
  switch (runMode) {
    case "inline":
      activateStarlingMonkeyDebug(context);
      break;
  }
}

export function deactivate() {}
