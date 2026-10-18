# Agent Notes

## Build

Build the project in `build/debug`:

```bash
cd build/debug
make -j$(nproc)
```

The `REMOTE_SOLVER` mode (the commented-out option and compile definition in `CMakeLists.txt`) is currently not relevant — keep it disabled and do not enable it in builds or tests.

## Run Server

From `build/debug`, run the server with environment variables from `../../../keys/yutovo-server.env` (outside the repo):

```bash
env $(grep -v '^#' ../../../keys/yutovo-server.env | xargs) ./src/yutovo-serverd
```

## Run Tests

From `build/debug`, run tests with environment variables from `../../../keys/yutovo-server.env` (outside the repo):

```bash
env $(grep -v '^#' ../../../keys/yutovo-server.env | xargs) ./test/yutovo-server_tests
```

The test executable starts an in-process Drogon instance, so you do not need to run the server beforehand. It adds the listener on `127.0.0.1:9001` (make sure the port is free) and configures the database client programmatically, so no manual changes to `config.json` are required.

## Agent Rules

- **Never commit without explicit user permission.** Do not run `git commit`, `git push`, `git reset`, `git rebase`, or any other git mutations unless explicitly asked to do so. Ask for confirmation each time when git mutations are needed.

## Style rules

- Functions must be inside classes.
- Continuation lines use only one indentation level.
- `break` after `case` is always on a new line.
