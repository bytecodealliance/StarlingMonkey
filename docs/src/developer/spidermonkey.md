# Developing Changes to SpiderMonkey

StarlingMonkey uses SpiderMonkey as its underlying JS engine, and by default, downloads build
artifacts from [a wrapper repository][wasi-embedding] around [our local SpiderMonkey
tree][gecko-dev]. That wrapper repository contains a SpiderMonkey commit-hash in a file, and its CI
jobs build the artifacts that StarlingMonkey downloads during its build.

This flow is optimized for ease of development of StarlingMonkey, and avoiding the need to build
SpiderMonkey locally, which requires some additional tools and is resource-intensive. However,
sometimes it is necessary or desirable to make modifications to SpiderMonkey directly, whether to
make fixes or optimize performance.

In order to do so, first clone the above two repositories, with `gecko-dev` (SpiderMonkey itself) as
a subdirectory to `spidermonkey-wasi-embedding`:

```console
git clone https://github.com/bytecodealliance/spidermonkey-wasi-embedding
cd spidermonkey-wasi-embedding/
git clone https://github.com/bytecodealliance/gecko-dev
```

and switch to the commit that we are currently using:

```console
git checkout `cat ../gecko-revision`
# now edit the source
```

Then make changes as necessary, eventually rebuilding from the `spidermonkey-wasi-embedding` root:

```console
cd ../ # back to spidermonkey-wasi-embedding
./rebuild.sh release
```

This will produce a `release/` directory with artifacts of the same form normally downloaded by
StarlingMonkey. So, finally, from within StarlingMonkey, set an environment variable
`SPIDERMONKEY_BINARIES`:

```console
export SPIDERMONKEY_BINARIES=/path/to/spidermonkey-wasi-embedding/release
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel 8
```

and use/test as described in [testing][testing] section.

[testing]: ../getting-started/testing.md
[wasi-embedding]: https://github.com/bytecodealliance/spidermonkey-wasi-embedding
[gecko-dev]: https://github.com/bytecodealliance/gecko-dev
