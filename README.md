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
[referência geral](docs/geral.md#gramatica) remain future work. Classes, interfaces, generics,
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
  -DARGOS_BUILD_TESTS=ON \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++

cmake --build build
```

Use a C++ driver (`g++` or `clang++`), not `gcc` or `clang`. If an existing
build reports undefined references to `std::cout`, `std::cerr` or `__cxa_*`,
reconfigure its cached toolchain (CMake 3.24+):

```bash
env PATH=/usr/bin:/bin cmake --fresh -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_AR=/usr/bin/ar -DCMAKE_RANLIB=/usr/bin/ranlib \
  -DARGOS_BUILD_TESTS=ON
cmake --build build
```

With older CMake, use a new build directory instead of `--fresh`.

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

- [Guia da linguagem](docs/guide/README.md): capítulos de uso e catálogo de diagnósticos.
- [Compilar, interpretar e executar](docs/teaching.md): construção, VM, execução nativa e plataformas.
- [Arquitetura](docs/architecture.md): organização do compilador e limites das fases.
- [Referência geral](docs/geral.md): MVP, roadmap, gramática, versão, testes e decisões históricas.

### Gerar assembly

Para salvar o assembly GNU/AT&T x86-64 em vez do executável:

```bash
./build/bin/argosc compile examples/minimal_arithmetic.argos -S -o /tmp/programa.s
cc /tmp/programa.s -o /tmp/programa
/tmp/programa
echo $? # 14
```

A forma curta `argosc -S arquivo.argos` também funciona e gera `arquivo.s`
no diretório atual. A opção `-S` usa o backend existente sobre a IR tipada e
validada, sem chamar `cc`. Montar e ligar o resultado exige uma toolchain
x86-64/Linux. O subconjunto suportado é o mesmo da compilação nativa.
