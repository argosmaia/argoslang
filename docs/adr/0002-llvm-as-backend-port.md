# ADR-0002: Treat LLVM as a backend adapter

- Status: Accepted
- Date: 2026-07-27

## Context

The language frontend must remain independent from a particular machine-code
technology. Direct LLVM types in the parser or semantic analyzer would create
architectural coupling.

## Decision

The frontend will lower source code to Argos-owned intermediate representations.
LLVM will later implement a backend port that consumes the lower IR.

## Consequences

- A native x86-64 backend can coexist with LLVM.
- Frontend tests do not need LLVM.
- Language semantics remain owned by ArgosLang.
- Lowering contracts must be carefully specified.
