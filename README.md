# ArgosLang Bootstrap

Initial C++23 compiler skeleton for ArgosLang.

Current vertical slice:

```text
source file → SourceManager → Lexer → Parser → AST → SemanticAnalyzer (Type Checker)
                                          ↓                 ↓
                                      AST Printer      diagnostics
```

The current frontend supports variables, functions, non-generic data protocols,
basic control flow, primitive and intrinsic `String` types, optionals, named
arguments, Unicode identifiers and source-backed diagnostics. The parser is
still handwritten; the canonical LR(1) generator and the broader grammar in
`docs/grammar-spec.md` remain future work. Classes, interfaces, generics,
modules, lambdas, ownership and foreign executors are not available yet.
`examples/null_safety.argos` declares its nominal types and passes `check`.

## Requirements on Ubuntu/Pop!_OS

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  clang \
  clang-format \
  clang-tidy \
  cmake \
  libicu-dev \
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
./build/bin/argosc lex examples/frontend_core.argos

# 2. Analisar a sintaxe (parsing da AST)
./build/bin/argosc parse examples/frontend_core.argos

# 3. Visualizar a Árvore de Sintaxe Abstrata (AST) formatada
./build/bin/argosc ast examples/frontend_core.argos

# 4. Executar a Análise Semântica e Validação de Tipos (Type Checker)
./build/bin/argosc check examples/frontend_core.argos

# 5. Compilar e executar o subconjunto nativo x86-64/Linux
./build/bin/argosc examples/minimal_literal.argos
./minimal_literal
echo $?  # 42
```

## Documentation & Guides

- [docs/guide/README.md](docs/guide/README.md): Guia principal por capítulos: instalação, sintaxe, tipos, funções, fluxo, REPL e diagnósticos.
- [docs/architecture.md](docs/architecture.md): Direção arquitetural e limites das fases.
- [docs/grammar-spec.md](docs/grammar-spec.md): Gramática e semântica planejadas.
- [docs/testing-and-compiler-guide.md](docs/testing-and-compiler-guide.md): Guia de testes do compilador.
- [docs/teachings/compilar.md](docs/teachings/compilar.md): Compilação de `.argos` e execução nativa do subconjunto disponível.
- [docs/teachings/interpretar.md](docs/teachings/interpretar.md): Estado e fluxo planejado da VM.
- [docs/teachings/executar-em-maquinas.md](docs/teachings/executar-em-maquinas.md): Hosts, alvos e portabilidade.
- [docs/mvp.md](docs/mvp.md): Contrato do subconjunto executável, VM, nativo e REPL.
- [docs/language-evolution.md](docs/language-evolution.md): Decisões e ordem de evolução após o MVP.
- [docs/argoslang-v0.0.1.md](docs/argoslang-v0.0.1.md): Referência de instalação e uso da versão v0.0.1.
