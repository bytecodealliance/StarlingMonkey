import {
  WorkspaceFolder,
  DebugConfiguration,
  ProviderResult,
  CancellationToken,
  commands,
  debug,
  DebugAdapterDescriptor,
  DebugAdapterDescriptorFactory,
  DebugAdapterInlineImplementation,
  DebugConfigurationProvider,
  DebugConfigurationProviderTriggerKind,
  DebugSession,
  ExtensionContext,
  Uri,
  window,
  workspace,
} from "vscode";
import { StarlingMonkeyDebugSession } from "./starlingMonkeyDebugger.js";
import { FileAccessor, IStarlingMonkeyRuntimeConfig } from "./starlingMonkeyRuntime.js";

export function activateStarlingMonkeyDebug(
  context: ExtensionContext,
  factory?: DebugAdapterDescriptorFactory
) {
  context.subscriptions.push(
    commands.registerCommand(
      "extension.starlingmonkey-debugger.runEditorContents",
      (resource: Uri) => {
        let targetResource = resource;
        if (!targetResource && window.activeTextEditor) {
          targetResource = window.activeTextEditor.document.uri;
        }
        if (targetResource) {
          debug.startDebugging(
            undefined,
            {
              type: "starlingmonkey",
              name: "Run File",
              request: "launch",
              program: targetResource.fsPath,
              component: "${workspaceFolder}/${command:AskForComponent}",
            },
            { noDebug: true }
          );
        }
      }
    ),
    commands.registerCommand(
      "extension.starlingmonkey-debugger.debugEditorContents",
      (resource: Uri) => {
        let targetResource = resource;
        if (!targetResource && window.activeTextEditor) {
          targetResource = window.activeTextEditor.document.uri;
        }
        if (targetResource) {
          debug.startDebugging(undefined, {
            type: "starlingmonkey",
            name: "Debug File",
            request: "launch",
            program: targetResource.fsPath,
            component: "${workspaceFolder}/${command:AskForComponent}",
            stopOnEntry: true,
          });
        }
      }
    )
  );

  context.subscriptions.push(
    commands.registerCommand(
      "extension.starlingmonkey-debugger.getProgramName",
      (config) => {
        return window.showInputBox({
          placeHolder:
            "Please enter the name of a JS file in the workspace folder",
          value: "index.js",
        });
      }
    )
  );

  context.subscriptions.push(
    commands.registerCommand(
      "extension.starlingmonkey-debugger.getComponent",
      (config) => {
        return window.showInputBox({
          placeHolder:
            "StarlingMonkey component in the workspace folder to use for debugging (e.g. 'starling.wasm')",
        });
      }
    )
  );

  const provider = new StarlingMonkeyConfigurationProvider();
  context.subscriptions.push(
    debug.registerDebugConfigurationProvider("starlingmonkey", provider)
  );

  context.subscriptions.push(
    debug.registerDebugConfigurationProvider(
      "starlingmonkey",
      {
        provideDebugConfigurations(
          folder: WorkspaceFolder | undefined
        ): ProviderResult<DebugConfiguration[]> {
          return [
            {
              name: "StarlingMonkey Launch",
              request: "launch",
              type: "starlingmonkey",
              program: "${file}",
              component: "${workspaceFolder}/${command:AskForComponent}",
            },
          ];
        },
      },
      DebugConfigurationProviderTriggerKind.Dynamic
    )
  );

  if (!factory) {
    factory = new InlineDebugAdapterFactory();
  }
  context.subscriptions.push(
    debug.registerDebugAdapterDescriptorFactory("starlingmonkey", factory)
  );
  if ("dispose" in factory) {
    context.subscriptions.push(<{ dispose(): any }>factory);
  }
}

class StarlingMonkeyConfigurationProvider
  implements DebugConfigurationProvider
{
  resolveDebugConfiguration(
    folder: WorkspaceFolder | undefined,
    config: DebugConfiguration,
    token?: CancellationToken
  ): ProviderResult<DebugConfiguration> {
    if (!config.type && !config.request && !config.name) {
      const editor = window.activeTextEditor;
      if (editor && editor.document.languageId === "javascript") {
        config.type = "starlingmonkey";
        config.name = "Launch";
        config.request = "launch";
        config.program = "${file}";
        config.component = "${file}";
        config.stopOnEntry = true;
      }
    }

    if (!config.program) {
      return window
        .showInformationMessage("Cannot find a program to debug")
        .then((_) => {
          return undefined;
        });
    }

    return config;
  }
}

export const workspaceFileAccessor: FileAccessor = {
  isWindows: typeof process !== "undefined" && process.platform === "win32",
  async readFile(path: string): Promise<Uint8Array> {
    let uri: Uri;
    try {
      uri = pathToUri(path);
    } catch (e) {
      return new TextEncoder().encode(`cannot read '${path}'`);
    }

    return await workspace.fs.readFile(uri);
  },
  async writeFile(path: string, contents: Uint8Array) {
    await workspace.fs.writeFile(pathToUri(path), contents);
  },
};

function pathToUri(path: string) {
  try {
    return Uri.file(path);
  } catch (e) {
    return Uri.parse(path);
  }
}

class InlineDebugAdapterFactory implements DebugAdapterDescriptorFactory {
  createDebugAdapterDescriptor(
    session: DebugSession
  ): ProviderResult<DebugAdapterDescriptor> {
    let config = workspace.getConfiguration("starlingmonkey");
    return new DebugAdapterInlineImplementation(
      new StarlingMonkeyDebugSession(
        workspaceFileAccessor,
        session.workspaceFolder ? session.workspaceFolder.uri.fsPath : '/',
        <IStarlingMonkeyRuntimeConfig><unknown>config
      )
    );
  }
}
