# Agent Notes

## Build

Build the project in `build/debug`. The `TEST` parameter is mandatory at configure time
(`cmake` fails without an explicit `-DTEST=ON` or `-DTEST=OFF`); `build/debug` uses
`TEST=ON` because it also builds the test suite:

```bash
cd build/debug
cmake -DCMAKE_BUILD_TYPE=Debug -DTEST=ON ../..
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

### Parenthesized expressions

Keep the contents of parentheses (function argument lists, conditions, initializers, etc.) on a single line when it fits. Only wrap to a new line if the expression would exceed **140 columns**.
When a parenthesized expression is wrapped, each continuation line uses the normal **4-space indent**; do not align arguments with the opening parenthesis.

```cpp
// CORRECT
void ShortFunction(int a, int b, int c);

void LongFunctionName(const std::string& first_argument, const std::string& second_argument,
    int third_argument);

auto result = SomeFunction(first_argument, second_argument,
    third_argument, fourth_argument);

if (condition_a && condition_b)
{
    // ...
}

// WRONG
void LongFunctionName(
    const std::string& first_argument,
    const std::string& second_argument,
    int third_argument);

void LongFunctionName(const std::string& first_argument,
                      const std::string& second_argument,
                      int third_argument);
```

### Spaces around brackets

Do not put spaces before or after square brackets `[]` and round brackets `()`:

```cpp
// CORRECT
int arr[10];
void foo(int a);
arr[0] = foo(1);

// WRONG
int arr [10];
void foo (int a);
arr [0] = foo (1);
```

### Comments

Comments that precede a function, method, or class (header/descriptive comments above the definition) start with a capital letter, e.g. `//Group of code paragraphs`, `//TextEquation`, `//Insert a text block into the document text`.

### Lambdas

Place the capture clause on a new line, indented by 4 spaces. Parameters, the `->` return type, and the opening brace follow the normal rules: parameters and return type stay on the same line as the capture clause, and the opening brace goes on its own line.

```cpp
// CORRECT
auto callback =
    [](int value) -> bool
    {
        return value > 0;
    };

auto reference =
    [&]() -> void
    {
        DoWork();
    };

// WRONG
auto callback = [](int value) -> bool {
    return value > 0;
};

auto callback =
    [](int value) -> bool {
    return value > 0;
};
```

### Expected HTML in tests

Verify generated HTML (SSR pages, exported documents) by comparing the **full** page body, not by substring spot checks (`body.find(...) != std::string::npos`). A full comparison catches wrong attributes, wrong nesting and missing tags, and serves as a golden for future changes.

Format the expected HTML literal with **one tag per line**: the deeper the nesting, the deeper the indentation. Adjacent string literals concatenate without any whitespace, so the comparison stays byte-exact while the expectation stays readable (the same convention as the ToHtml checks in the yutovo-editor tests).

The closing parenthesis of the call that receives a multi-line expected HTML literal goes on its own line, at the indentation of the statement, followed by the `<<` actual-value output:

```cpp
std::string body(r->getBody());
ASSERT_TRUE(body ==
    "<html lang=\"en\">"
        "<head>"
            "<title>Force</title>"
            "<meta name=\"description\" content=\"...\">"
        "</head>"
        "<body>"
        "<div id=q-app>"
            "<canvas tabindex=\"-1\">"
            "<article>"
                "<h1>Force</h1>"
                "<p>"
                    "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
                        "<mrow>"
                            "<mi>1</mi>"
                            "<mi>N</mi>"
                        "</mrow>"
                    "</math>"
                "</p>"
            "</article>"
            "</canvas>"
        "</div>"
        "</body>"
    "</html>\n"
    ) << body;
```

Repeating identical siblings (e.g. a table of unit rows) may stay one line per sibling. Absence checks (`find(...) == std::string::npos`) are acceptable only for negative assertions (no content exposed) or exact occurrence counts.

