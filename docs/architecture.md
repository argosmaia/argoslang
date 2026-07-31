# ArgosLang — Architecture Baseline

## Architectural style

ArgosLang starts as a modular monolith organized as a compiler pipeline.

The compiler is not divided into services. Its modules communicate through
in-memory contracts and immutable value objects.

## Dependency rule

```text
driver
  ↓
application pipeline
  ↓
frontend domain
  ├── source
  ├── diagnostics
  ├── lexer
  ├── parser
  └── semantic analysis
       ↓
intermediate representations
       ↓
backend ports
       ├── LLVM adapter
       └── native x86-64 adapter
```

Inner modules must not include headers from outer modules.

## Initial bounded modules

### source

Owns source files, locations, spans, line mapping and future source caching.

### diagnostics

Owns structured compiler diagnostics. It does not print directly to a terminal.

### lexer

Transforms source text into tokens. It depends on source and diagnostics.

### driver

Owns the command-line process, exit codes and human-readable presentation.

## Design rules

1. No global mutable compiler state.
2. No printing from the lexer.
3. Every token and diagnostic carries a source span.
4. Operators use maximal munch.
5. Invalid input produces diagnostics rather than process crashes.
6. The core frontend does not depend on LLVM.
7. Tests target public contracts, not private implementation details.
8. New compiler phases are added as modules, not as conditions in the driver.
