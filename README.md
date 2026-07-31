# ArgosLang Bootstrap

Initial C++23 compiler skeleton for ArgosLang.

Current vertical slice:

```text
source file → SourceManager → Lexer → Token stream → terminal
                                  ↓
                            diagnostics
```

## Requirements on Ubuntu/Pop!_OS

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  clang \
  clang-format \
  clang-tidy \
  cmake \
  ninja-build \
  git \
  gdb \
  lldb
```

## Configure and build

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DARGOS_BUILD_TESTS=ON

cmake --build build
```

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Run the first compiler command

```bash
./build/bin/argosc lex examples/null_safety.argos
```

## Build with sanitizers

```bash
cmake -S . -B build-sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DARGOS_BUILD_TESTS=ON \
  -DARGOS_ENABLE_SANITIZERS=ON

cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

## Module map

```text
compiler/
├── include/argos/
│   ├── diagnostics/
│   ├── lexer/
│   └── source/
└── src/
    ├── diagnostics/
    ├── driver/
    ├── lexer/
    └── source/

tests/
└── lexer/

docs/
├── architecture.md
└── adr/
```

## Next increment

The next vertical slice should add:

```text
tokens → parser → expression AST → AST printer
```

It should initially parse:

- identifiers and literals;
- postfix `!` and `!!`;
- member access `.` and safe member access `?.`;
- coalescing `??`;
- assignment operators `=`, `?=` and `??=`;
- parenthesized expressions.

Do not add LLVM before this grammar has tests and stable diagnostics.
