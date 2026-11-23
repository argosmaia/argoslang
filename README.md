# ArgosLang Bootstrap

C++23 compiler for the executable ArgosLang MVP.

Current vertical slice:

```text
source → lexer → hybrid parser → AST → semantics → typed IR → validation
                                                        → optimization → validation → VM / native backend
```

The compiler supports variables, functions, classes, single inheritance,
interfaces (`by`), virtual dispatch, overload/override, `super`, immutable `data`,
sealed hierarchies and `permited` access lists,
computed properties, lambdas/closures, optionals and homogeneous arrays.
These features execute in the VM and on native x86-64/Linux; see the
[language guide](docs/guide/README.md) for syntax and current limits.
`protocol` has been removed; use `interface` for contracts and `data` for
immutable aggregates. Expressions use a canonical LR(2) engine; declarations,
types, prefixes and postfixes still use handwritten parsing. Generics, automatic
imports, Rust-style ownership/borrowing, scope-based destruction and foreign executors
remain future work in the [general reference](docs/geral.md#gramatica).

## Current implementation and limits

- Declarations use `let name` or `let mut name`. The old `var` spelling is rejected.
- VM and x86-64/Linux execute Unicode `char`, `byte`/`sbyte`/`short`/`ushort`,
  nested arrays and bound method references. Narrow integer overflow traps;
  `int` arithmetic wraps modulo 2^32, including negation of the minimum value.
- `float` values round to binary32 at expression/assignment boundaries; the IR
  transports these values in binary64 words. `double` remains binary64.
- IR verification rejects missing terminators, undefined register/slot reads,
  invalid control-flow edges, unknown opcodes and duplicate entry points.
- `-O1` (default) folds local integer constants and branches, removes unreachable
  blocks and unused non-trapping results. `-O0` disables optional optimizations.
- `unsafe` gates `raw_alloc(int) -> ptr`, `raw_free(ptr) -> void`,
  `raw_load(ptr, int) -> byte` and `raw_store(ptr, int, byte) -> void`.
  A module may use manual raw memory or the existing arena, not both.
  The arena is **not** Rust ownership/borrowing.
- `--target=avr -S` emits scalar AVR assembly for ATmega328P. Functions,
  six 32-bit arguments, calls, integer arithmetic, remainder, comparisons and
  control flow are supported; heap, floating point and I/O are rejected.
  Linking requires `avr-gcc`; hardware execution is not covered by the host tests.
- Full LR(2) parsing of declarations/types, all 64-bit/unsigned integer types,
  C/C++ pointer/FFI/MMIO facilities and Rust-style lifetimes remain unfinished.

Examples: `examples/minimal_extended_values.argos`,
`examples/minimal_raw_memory.argos`, `examples/minimal_avr.argos`.

```bash
argosc run examples/minimal_extended_values.argos -O0
argosc compile examples/minimal_raw_memory.argos -O1 -o raw-memory
argosc compile examples/minimal_avr.argos --target=avr -S -o firmware.s
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
  libicu-dev \
  libreadline-dev \
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

A POO inclui composição múltipla com `with`, acesso por pacote com `default`,
tipos aninhados e records `data` com igualdade, hash e texto gerados. Veja as
[regras e exemplos](docs/guide/10-classes-e-interfaces.md).
