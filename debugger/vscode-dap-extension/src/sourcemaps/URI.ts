/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the same folder for license information.
 *--------------------------------------------------------------------------------------------*/

import * as URL from "url";
import * as PathUtils from "./pathUtilities";

function isWindows(absPath: string): boolean {
  return /^[a-zA-Z]\:\\/.test(absPath);
}

function stripFirst(path: string, c: string) {
  return path[0] === c ? path.substr(1) : path;
}

function stripLast(path: string, c: string) {
  return path[path.length - 1] === c ? path.substr(0, path.length - 1) : path;
}

export class URI {
  private _uri!: string;
  private _u!: URL.Url;

  /**
   * Creates a file URI from the given file path.
   * If path is relative, an absolute base path must be provided as well.
   * If base is missing or if base is not absolute, an exception is thrown.
   */
  public static file(path: string, base?: string) {
    if (typeof path !== "string") {
      throw new Error("string expected");
    }

    if (!PathUtils.isAbsolutePath(path)) {
      if (base) {
        if (PathUtils.isAbsolutePath(base)) {
          if (isWindows(base)) {
            path = stripLast(base, "\\") + "\\" + stripFirst(path, "\\");
          } else {
            path = stripLast(base, "/") + "/" + stripFirst(path, "/");
          }
        } else {
          throw new Error("base path not absolute");
        }

        //path = PathUtils.makePathAbsolute(base, path);
      } else {
        throw new Error("base path missing");
      }
    }

    if (isWindows(path)) {
      // is windows
      path = path.replace(/\\/g, "/");
    }

    // simplify '/./' -> '/'
    path = path.replace(/\/\.\//g, "/");

    if (path[0] !== "/") {
      path = "/" + path;
    }

    path = encodeURI("file://" + path);

    const u = new URI();
    u._uri = path;
    u._u = URL.parse(path);
    return u;
  }

  /**
   * Creates a URI from the given string.
   */
  public static parse(uri: string, base?: string) {
    if (
      uri.indexOf("http:") === 0 ||
      uri.indexOf("https:") === 0 ||
      uri.indexOf("file:") === 0 ||
      uri.indexOf("data:") === 0
    ) {
      const u = new URI();
      u._uri = uri;
      u._u = URL.parse(uri);
      return u;
    }
    return URI.file(uri, base);
  }

  constructor() {}

  uri(): string {
    return this._uri;
  }

  isFile(): boolean {
    return this._u.protocol === "file:";
  }

  filePath(): string {
    let path = <string>this._u.path;
    path = decodeURI(path);

    if (/^\/[a-zA-Z]\:\//.test(path)) {
      path = path.substr(1); // remove additional '/'
      path = path.replace(/\//g, "\\"); // convert slashes to backslashes
    }
    return path;
  }

  isData() {
    return (
      this._u.protocol === "data:" &&
      this._uri.indexOf("application/json") > 0 &&
      this._uri.indexOf("base64") > 0
    );
  }

  data(): string | null {
    const pos = this._uri.lastIndexOf(",");
    if (pos > 0) {
      return this._uri.substr(pos + 1);
    }
    return null;
  }

  isHTTP(): boolean {
    return this._u.protocol === "http:" || this._u.protocol === "https:";
  }
}
