# Agent Notes

## Build

Build the project in `build/debug`:

```bash
cd build/debug
make -j$(nproc)
```

## Run Server

From `build/debug`, run the server with environment variables from `yutovo-server.env`:

```bash
env $(grep -v '^#' ../../yutovo-server.env | xargs) ./src/yutovo-serverd
```

## Run Tests

From `build/debug`, run tests with environment variables from `yutovo-server.env`:

```bash
env $(grep -v '^#' ../../yutovo-server.env | xargs) ./test/yutovo-server_tests
```

For tests to work, `config.json` must have an HTTP listener enabled (for example, on `127.0.0.1:9001`). The default `src/config.json` has the `listeners` section commented out; enable it before running tests.
