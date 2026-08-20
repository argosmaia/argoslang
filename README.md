# ArgosLang Bootstrap

Initial C++23 compiler skeleton for ArgosLang.

Current vertical slice:

```text
source file → SourceManager → Lexer → Parser → AST → SemanticAnalyzer (Type Checker)
                                          ↓                 ↓
                                      AST Printer      diagnostics
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

## Run the compiler CLI commands

```bash
# 1. Analisar a sequência de tokens lexicais
./build/bin/argosc lex examples/null_safety.argos

# 2. Analisar a sintaxe (parsing da AST)
./build/bin/argosc parse examples/null_safety.argos

# 3. Visualizar a Árvore de Sintaxe Abstrata (AST) formatada
./build/bin/argosc ast examples/null_safety.argos

# 4. Executar a Análise Semântica e Validação de Tipos (Type Checker)
./build/bin/argosc check examples/null_safety.argos
```

## Documentation & Guides

- [docs/grammar-spec.md](file:///home/dgti/Downloads/argoslang-bootstrap/docs/grammar-spec.md): Especificação formal EBNF, tipos primitivos de tamanho fixo, sem GC e modelo bare-metal.
- [docs/testing-and-compiler-guide.md](file:///home/dgti/Downloads/argoslang-bootstrap/docs/testing-and-compiler-guide.md): Guia de testes e arquitetura de compilação.
