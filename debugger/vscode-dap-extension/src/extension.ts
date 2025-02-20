import { Server, createServer, AddressInfo } from "net";
import {
  ExtensionContext,
  DebugAdapterDescriptorFactory,
  DebugSession,
  DebugAdapterExecutable,
  ProviderResult,
  DebugAdapterDescriptor,
  DebugAdapterServer,
  workspace,
} from "vscode";
import { StarlingMonkeyDebugSession } from "./starlingMonkeyDebugger.js";
import {
  activateStarlingMonkeyDebug,
  workspaceFileAccessor,
} from "./activateStarlingMonkeyDebugger.js";
import { IStarlingMonkeyRuntimeConfig } from "./starlingMonkeyRuntime.js";

const runMode: "server" | "inline" = "inline";

export function activate(context: ExtensionContext) {
  switch (runMode) {
    case "server":
      activateStarlingMonkeyDebug(
        context,
        new StarlingMonkeyDebugAdapterServerDescriptorFactory()
      );
      break;

    case "inline":
      activateStarlingMonkeyDebug(context);
      break;
  }
}

export function deactivate() {}

class StarlingMonkeyDebugAdapterServerDescriptorFactory
  implements DebugAdapterDescriptorFactory
{
  private server?: Server;

  createDebugAdapterDescriptor(
    session: DebugSession,
    executable: DebugAdapterExecutable | undefined
  ): ProviderResult<DebugAdapterDescriptor> {
    if (!this.server) {
      this.server = createServer((socket) => {
        let config = workspace.getConfiguration("starlingmonkey");
        const debugSession = new StarlingMonkeyDebugSession(
          workspaceFileAccessor,
          session.workspaceFolder ? session.workspaceFolder.uri.fsPath : "/",
          <IStarlingMonkeyRuntimeConfig><unknown>config
        );
        debugSession.setRunAsServer(true);
        debugSession.start(socket as NodeJS.ReadableStream, socket);
      }).listen(0);
    }

    return new DebugAdapterServer((this.server.address() as AddressInfo).port);
  }

  dispose() {
    if (this.server) {
      this.server.close();
    }
  }
}
