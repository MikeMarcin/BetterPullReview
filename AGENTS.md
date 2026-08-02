# BetterPullReview Agent Guide

This file applies to the repository unless a nearer `AGENTS.md` adds more
specific instructions.

## Read first

1. `README.md` and `CONTRIBUTING.md`
2. `_coding-standard/STANDARD.md`
3. `docs/architecture/product-contract.md`
4. `docs/architecture/system-architecture.md`
5. The current RFC in `docs/rfcs/`
6. The live board and current card in `docs/issues/`
7. The nearest nested `AGENTS.md`

## Governing architecture

The core decides. The host performs. Qt presents.

| Scope | Owner |
|---|---|
| `src/core/` | Deterministic policy, compact interaction state, typed transitions, request identity, semantic deltas, diagnostics |
| `src/application/` | Effects, bounded observations, serialized completion lane, cancellation, lifecycle |
| `src/adapters/qt/` | Qt events, widgets, actions, models, focus, accessibility, rendering, incremental projection |
| `src/adapters/{github,git,sqlite,ai}` | Narrow application-host capability ports; no provider types cross inward |
| `src/app/` | Concrete composition and application entry point |
| `tests/` | Core, host, contract, Qt, end-to-end, fixture, benchmark, and fuzz evidence |
| `packaging/` | Distribution metadata and release-time platform policy |

The functional core must not include Qt, provider, filesystem, process, clock,
thread, timer, network, database, credential, or hidden-global dependencies.
A widget or Qt model must not perform GitHub, Git, SQLite, AI, filesystem,
network, or process effects.

## Workflow

- RFCs close sequentially in the canonical index order.
- Issue folder placement is status. `done/` is terminal.
- Allocate issue IDs from `docs/issues/NEXT_ID`.
- Commit each completed issue slice intentionally.
- Use fixtures or designated safe test repositories for remote mutations.
- Never commit credentials, private repository content, transient inspection
  artifacts, build output, or secret-bearing logs.

## Commands

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --build --preset dev --target coding-format-check coding-tidy

cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure

python3 scripts/verify_documentation.py --root .
```

Before RFC closeout, run the documentation gate over the immutable RFC base,
the complete verification matrix from clean state, the selected runtime and
package checks, and `git diff --check`.
