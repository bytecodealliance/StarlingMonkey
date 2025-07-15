# Project workflow using `just`

## Getting Started

The justfile provides a streamlined interface for executing common project-specific tasks. To
install just, you can use the command `cargo install just` or `cargo binstall just`. Alternatively,
refer to the official [installation instructions][just-install] for your specific system.

Once installed, navigate to the project directory and run `just` commands as needed. For instance,
the following commands will configure a default `cmake-build-debug` directory and build the project.

```console
just build
```

To load a JS script during componentization and serve its output using `Wasmtime`, run:

```console
just serve <filename>.js
```

To build and run integration tests run:

```console
just test
```

To build and run Web Platform Tests run:

```console
just wpt-setup # prepare WPT hosts
just wpt-test # run all tests
just wpt-test console/console-log-symbol.any.js # run specific test
```

To view a complete list of available recipes, run:

```console
just --list
```

> [!NOTE]
> By default, the CMake configuration step is skipped if the build directory already exists.
> However, this can sometimes cause issues if the existing build directory was configured for a
> different target. For instance:
>
> - Running `just build` creates a build directory for the default target,
> - Running `just wpt-build` afterward may fail because the WPT target hasn’t been configured in the
>   existing build directory.
>
> To resolve this, you can force cmake to reconfigure the build directory by adding the
> `reconfigure=true` parameter. For example:
>
> ```console
> just reconfigure=true wpt-build
> ```

## Customizing build

The default build mode is debug, which automatically configures the build directory to
`cmake-build-debug`. You can switch to a different build mode, such as release, by specifying the
mode parameter. For example:

```console
just mode=release build
```

This command will set the build mode to release, and the build directory will automatically change
to `cmake-build-release`.

If you want to override the default build directory, you can use the `builddir` parameter.

```console
just builddir=mybuilddir mode=release build
```

This command configures CMake to use `mybuilddir` as the build directory and sets the build mode to
`release`.

## Starting the WPT Server

You can also start a Web Platform Tests (WPT) server with:

```console
just wpt-server
```

After starting the server, individual tests can be run by sending a request with the test name to
the server instance. For example:

```console
curl http://127.0.0.1:7676/console/console-log-symbol.any.js

```

[just-install]: https://github.com/casey/just?tab=readme-ov-file#installation
