# BetterPullReview

BetterPullReview is a native, local, cross-platform workbench for reviewing
GitHub pull requests. GitHub remains the sole collaborative source of truth;
BetterPullReview stores only private workflow state and derived projections
that GitHub does not represent.

The current implementation is the RFC-BPR-0001 repository foundation: a C++23
deterministic functional core, an imperative application host, a Qt 6 Widgets
presentation adapter, and an explicit composition root. It renders and
exercises a native empty-state application. GitHub/GHES access, real pull
request loading, diffs, review workflows, and packaging arrive in later RFCs
and are not claimed by this foundation.

## Architecture

> The core decides. The host performs. Qt presents.

- `src/core/` owns deterministic semantic state, transitions, request identity,
  deltas, diagnostics, and shared/private authority policy. It has no Qt or
  effectful dependency.
- `src/application/` owns bounded observations, typed request execution,
  serialized completion delivery, cancellation, and lifecycle.
- `src/adapters/qt/` translates Qt events into semantic intents and applies
  projections incrementally.
- `src/app/` is the only composition root that knows the concrete pieces.

See the [implementation architecture](docs/architecture/foundation-implementation.md)
and [normative system architecture](docs/architecture/system-architecture.md).

## Build and test

Prerequisites are CMake 3.25+, Ninja, a C++23 compiler, and Qt 6.8+ for the
native application.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Build and test the semantic core and application host without Qt:

```sh
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure
```

Run the application:

```sh
build/dev/src/app/better_pull_review
```

Detailed commands, targets, runtime audits, and evidence expectations are in
[Build and Test](docs/development/build-and-test.md).

## Repository workflow

- [RFC index](docs/rfcs/README.md)
- [Issue board](docs/issues/README.md)
- [Contributing](CONTRIBUTING.md)
- [Security baseline](docs/security/foundation-security.md)
- [Verification plan](docs/verification/RFC-BPR-0001.md)
