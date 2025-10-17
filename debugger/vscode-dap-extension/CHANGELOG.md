# Changelog

## [0.2.1](https://github.com/bytecodealliance/StarlingMonkey/compare/starlingmonkey-debugger-v0.2.0...starlingmonkey-debugger-v0.2.1) (2025-10-17)


### Bug Fixes

* **debugger:** Fix path normalization in debugger sourcemaps handling ([#279](https://github.com/bytecodealliance/StarlingMonkey/issues/279)) ([afcf222](https://github.com/bytecodealliance/StarlingMonkey/commit/afcf222f512eb211d1b29d7a427fc8db6dd27f84))

## 0.2.0 (2025-08-20)

Initial preview release of the extension, supporting the usual basics of debugging: setting breakpoints, changing values on the stack, stepping over and into expressions, showing stacks, etc, both for pure JS and for languages compiled to JS, provided they generate useful sourcemaps.

As the [README.md](README.md) file says, things might break in surprising and unfortunate ways. For the most part, the extension should work well, though.
