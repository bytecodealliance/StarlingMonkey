// import { StarlingMonkeyDebugSession } from "./starlingMonkeyDebugger.js";
// import { promises as fs } from "fs";
// import * as Net from "net";
// import { FileAccessor } from "./starlingMonkeyRuntime.js";

// const fsAccessor: FileAccessor = {
//   isWindows: process.platform === "win32",
//   readFile(path: string): Promise<Uint8Array> {
//     return fs.readFile(path);
//   },
//   writeFile(path: string, contents: Uint8Array): Promise<void> {
//     return fs.writeFile(path, contents);
//   },
// };

// const args = process.argv.slice(2);
// if (args.length < 1) {
//   console.error("Usage: node debugAdapter.js [--server=<port>] <workspace>");
//   process.exit(1);
// }
// const workspace = <string>args.pop();
// let port = 0;
// args.forEach(function (val) {
//   const portMatch = /^--server=(\d{4,5})$/.exec(val);
//   if (portMatch) {
//     port = parseInt(portMatch[1], 10);
//   }
// });

// // TODO: actually start the debug session once passing the correct configuration has been implemented.
// // if (port > 0) {
// //   console.error(`waiting for debug protocol on port ${port}`);
// //   Net.createServer((socket) => {
// //     console.error(">> accepted connection from client");
// //     socket.on("end", () => {
// //       console.error(">> client connection closed\n");
// //     });
// //     const session = new StarlingMonkeyDebugSession(fsAccessor, workspace);
// //     session.setRunAsServer(true);
// //     session.start(socket, socket);
// //   }).listen(port);
// // } else {
// //   const session = new StarlingMonkeyDebugSession(fsAccessor, workspace);
// //   process.on("SIGTERM", () => {
// //     session.shutdown();
// //   });
// //   session.start(process.stdin, process.stdout);
// // }
