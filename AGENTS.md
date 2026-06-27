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

The test executable starts an in-process Drogon instance, so you do not need to run the server beforehand. It adds the listener on `127.0.0.1:9001` (make sure the port is free) and configures the database client programmatically, so no manual changes to `config.json` are required.
