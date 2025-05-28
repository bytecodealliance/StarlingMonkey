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
  InputBoxOptions,
} from "vscode";
import { StarlingMonkeyDebugSession } from "./starlingMonkeyDebugger.js";
import { FileAccessor, IStarlingMonkeyRuntimeConfig } from "./starlingMonkeyRuntime.js";

// function registerCommand(context: ExtensionContext, name: String, handler: (resource: Uri | undefined) => Promise<void>) {
//   const fullName = `extension.starlingmonkey-debugger.${name}`;
//   const subscription = commands.registerCommand(fullName, handler);
//   context.subscriptions.push(subscription);
// }

function registerInputCommand(context: ExtensionContext, name: String, options?: InputBoxOptions) {
  const fullName = `extension.starlingmonkey-debugger.${name}`;
  const handler = async () => await window.showInputBox(options);
  const subscription = commands.registerCommand(fullName, handler);
  context.subscriptions.push(subscription);
}

// function defaultToActiveDocument(resource: Uri | undefined): Uri | undefined {
//   if (!resource && window.activeTextEditor) {
//     return window.activeTextEditor.document.uri;
//   }
//   return resource;
// }

// async function runEditorContents(resource: Uri | undefined): Promise<void> {
//   const targetResource = defaultToActiveDocument(resource);
//   if (targetResource) {
//     await debug.startDebugging(
//       undefined,
//       {
//         type: "starlingmonkey",
//         name: "Run File",
//         request: "launch",
//         program: targetResource.fsPath,
//         component: "${workspaceFolder}/${command:AskForComponent}",
//       },
//       { noDebug: true }
//     );
//   }
// }

// async function debugEditorContents(resource: Uri | undefined): Promise<void> {
//   const targetResource = defaultToActiveDocument(resource);
//   if (targetResource) {
//     await debug.startDebugging(undefined, {
//       type: "starlingmonkey",
//       name: "Debug File",
//       request: "launch",
//       program: targetResource.fsPath,
//       component: "${workspaceFolder}/${command:AskForComponent}",
//       stopOnEntry: true,
//     });
//   }
// }

export function activateStarlingMonkeyDebug(
  context: ExtensionContext,
  factory?: DebugAdapterDescriptorFactory
) {
  // TODO: these do not work.  Are they needed or just copied from the
  // sample "mock" debugger?
  // registerCommand(context, "runEditorContents", runEditorContents);
  // registerCommand(context, "debugEditorContents", debugEditorContents);

  registerInputCommand(context, "getProgramName", {
    placeHolder: "Please enter the name of a JS file in the workspace folder",
    value: "index.js",
  });
  registerInputCommand(context, "getComponent", {
    placeHolder: "StarlingMonkey component in the workspace folder to use for debugging (e.g. 'starling.wasm')",
  });

  const provider = new StarlingMonkeyConfigurationProvider();
  context.subscriptions.push(
    debug.registerDebugConfigurationProvider("starlingmonkey", provider)
  );

  context.subscriptions.push(
    debug.registerDebugConfigurationProvider(
      "starlingmonkey",
      {
        provideDebugConfigurations(
          _folder: WorkspaceFolder | undefined
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
  if (isDisposable(factory)) {
    context.subscriptions.push(factory);
  }
}

function isDisposable(obj: any): obj is { dispose(): any } {
  return typeof obj["dispose"] === "function";
}

class StarlingMonkeyConfigurationProvider
  implements DebugConfigurationProvider
{
  resolveDebugConfiguration(
    _folder: WorkspaceFolder | undefined,
    config: DebugConfiguration,
    _token?: CancellationToken
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
        session.workspaceFolder ? session.workspaceFolder.uri.fsPath : '/',  // TODO: this fucks up normalisation though
        <IStarlingMonkeyRuntimeConfig><unknown>config
      )
    );
  }
}
