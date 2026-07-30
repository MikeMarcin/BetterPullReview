# BetterPullReview

BetterPullReview is a native, local, cross-platform workbench for reviewing
GitHub pull requests. GitHub remains the sole collaborative source of truth;
BetterPullReview stores only private workflow state and derived projections
that GitHub does not represent.

The current implementation includes the RFC-BPR-0001 repository foundation
and RFC-BPR-0002's implemented public read-only GitHub path. The public
executable composes explicit GitHub.com and optional GHES 3.18 profiles,
native HTTPS, the target platform's credential vault, the normalized provider
and application host, and the Qt 6 Widgets presentation. A pull request's
metadata and conversations can traverse the real
QAction-to-provider-to-core-acceptance-to-rendered-window path.

GitHub CLI choices remain visible but deliberately unavailable: their trust
contract is still open, so selecting one fails closed before starting a
process. Native credentials are entered through a Qt-owned password input,
stored under a hostname-bound authority, and exact-deleted before an orderly
public-application shutdown completes. There is no durable credential
selector; a crash can leave an item that BetterPullReview cannot enumerate or
recover. Diff, review-mutation, private-ledger, and packaging work remains in
later RFCs. The GitHub window now presents that path through a compact,
responsive hierarchy with explicit request states, retained accepted content
during refresh, and read-only conversation rendering. It inherits the active
native palette, style, and font without forcing a palette, style, or QSS, and
responds to application appearance and font changes. User-selectable
appearance profiles remain RFC-BPR-0006 work.

## Architecture

> The core decides. The host performs. Qt presents.

- `src/core/` owns deterministic semantic state, transitions, request identity,
  deltas, diagnostics, and shared/private authority policy. It has no Qt or
  effectful dependency.
- `src/application/` owns bounded observations, typed request execution,
  serialized completion delivery, cancellation, credential lifecycle, and
  shutdown quiescence.
- `src/adapters/github/` owns validated profiles, normalized GitHub.com/GHES
  contracts, native HTTPS, the fail-closed CLI broker, and platform-vault
  adapters.
- `src/adapters/qt/` translates Qt events into semantic intents and applies
  projections incrementally. It also owns credential input, focus,
  accessibility, layout, and appearance.
- `src/app/` is the only composition root that selects concrete profiles,
  transports, vaults, workers, the provider, host, and Qt presentation.

See the [GitHub host implementation
architecture](docs/architecture/github-host-implementation.md) and
[normative system architecture](docs/architecture/system-architecture.md).

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

Add an explicit GHES 3.18 hostname when needed:

```sh
build/dev/src/app/better_pull_review --ghes-host ghe.example.com
```

The GitHub.com native HTTPS profile is built in. A supplied GHES hostname is
validated as explicit user configuration; provider content and redirects
cannot create a profile. The application prompts for native-HTTPS credential
material only after the user activates that profile.

Detailed commands, targets, runtime audits, and evidence expectations are in
[Build and Test](docs/development/build-and-test.md).

All configure, build, test, package, release, and verification work is local
or target-native. GitHub Actions has zero role.

## Repository workflow

- [RFC index](docs/rfcs/README.md)
- [Issue board](docs/issues/README.md)
- [Contributing](CONTRIBUTING.md)
- [Security baseline](docs/security/foundation-security.md)
- [RFC-BPR-0002 verification ledger](docs/verification/RFC-BPR-0002.md)
