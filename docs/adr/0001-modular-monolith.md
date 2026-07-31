# ADR-0001: Use a modular monolith

- Status: Accepted
- Date: 2026-07-27

## Context

A compiler executes a tightly coupled, mostly sequential transformation pipeline.
Network boundaries would add serialization, latency and operational failure modes
without creating useful independence.

## Decision

Implement ArgosLang as a single process composed of modules with explicit
compile-time dependency direction.

## Consequences

- Fast in-memory phase communication.
- Simple local debugging.
- One deployment artifact for the compiler driver.
- Module boundaries must be protected through CMake targets and public headers.
