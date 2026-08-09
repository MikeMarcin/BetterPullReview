# BetterPullReview

BetterPullReview is a native, local, cross-platform workbench for reviewing
GitHub pull requests. GitHub remains the sole collaborative source of truth;
BetterPullReview stores only private workflow state and derived projections
that GitHub does not represent.

The current implementation includes the RFC-BPR-0001 repository foundation,
RFC-BPR-0002's implemented public read-only GitHub path, and RFC-BPR-0003's
available-platform retained-revision and epoch-comparison path. RFC-BPR-0004's
available-platform implementation now adds deterministic diff computation,
one canonical mapping/group/navigation/detail model, one comparison-space
scroll authority, a custom link map, and the selected rendered workbench;
final RFC closeout remains tracked separately. The public executable composes
explicit GitHub.com and optional GHES 3.18 profiles,
native HTTPS as the sole milestone-one production authentication transport,
the target platform's credential vault, the normalized provider and
application hosts, an isolated installed-Git object engine, an
authority-partitioned private cache and epoch catalog, and the Qt 6 Widgets
presentation. Pull-request metadata, conversations, and accepted revision
history can traverse the real `QAction` -> provider -> core acceptance ->
rendered-window path.

The final-state review in BPR-0225 narrows the workbench claim. The shipped
window is RFC-BPR-0005's shell and its epoch controls are connected, but its
workbench callbacks are empty, its workbench host publishes into a
non-rendering intermediate sink, and accepted pull-request state is not offered
to that host. A selected diff can currently be requested only by a
tests-enabled fixture helper, not by a user-selected file. BPR-0226 and
BPR-0227 own those missing public wires. RFC-BPR-0006 through RFC-BPR-0011 have
substantial tested component implementations, but their remaining work is not
limited to composition: theme customization/import, durable notes/heads/defect
metadata, queue filters/sorting, and complete agent context/process lifecycle
are also absent. BPR-0228 through BPR-0240 track the exact component,
resilience, and public-composition slices.

GitHub CLI choices remain visible but deliberately unavailable: their trust
contract is outside milestone-one product scope, so selecting one returns the
fixed typed `cli_unavailable` result with zero CLI executable/configuration
discovery or CLI process launch. Positive CLI enablement is deferred to
[RFC-BPR-0015](docs/rfcs/future/RFC-BPR-0015-github-cli-production-enablement.md).
Native credentials are entered through a Qt-owned password input, stored under
a hostname-bound authority, and exact-deleted before an orderly
public-application shutdown completes. There is no durable credential
selector; a crash can leave an item that BetterPullReview cannot enumerate or
recover. The Git history path retains only revisions BetterPullReview actually
observed. Private refs keep an observed old head comparable after a force-push
while that epoch remains retained rather than explicitly pruned or
cache-deleted, but never-observed history cannot be reconstructed. Cache and
catalog identities are opaque and isolated by
profile, account, and repository; hard limits refuse new retention instead of
automatically pruning prior history. The current public UI exposes epoch
selection plus a secondary `Local history maintenance` menu. Pruning captures
the exact selected epoch IDs; whole-cache deletion captures the exact current
repository-cache scope. Both require an explicit Qt confirmation whose default
and Escape action are Cancel, leave durable tombstones, and state that GitHub
and developer worktrees are not modified. Users must not edit the private Git
repository or SQLite catalog by hand.

Review mutation and private-ledger workflows remain later public-composition
work. The shipped window is `workbench_window`, not the deleted
`github_window`; it presents the compact pull-request shell, request states,
retained accepted content, read-only conversations, and connected epoch
controls. The natural-layout selected-file comparison, workbench navigation,
drawers, and detail components are directly exercised in harnesses, but a
reviewer cannot currently reach them because the shell-to-host wires described
above are absent. Appearance profiles and syntax components remain composed
when their durable settings store cannot open. The window then starts from
System defaults, labels appearance changes as session-only, and keeps syntax
classification and profile switching available; a restart under the same
failure returns to those defaults. BPR-0237 owns the still-missing semantic
customization and safe theme import.

## Architecture

> The core decides. The host performs. Qt presents.

- `src/core/` owns deterministic semantic state, transitions, request identity,
  deltas, diagnostics, and shared/private authority policy. It has no Qt or
  effectful dependency.
- `src/application/` owns bounded observations, typed request execution,
  serialized completion delivery, cancellation, credential lifecycle, epoch
  jobs, recovery, and shutdown quiescence.
- `src/adapters/github/` owns validated profiles, normalized GitHub.com/GHES
  contracts, native HTTPS, the fail-closed CLI broker, and platform-vault
  adapters.
- `src/adapters/git/` and `src/adapters/sqlite/` own the narrow installed-Git,
  private bare-cache/ref, and durable epoch-catalog mechanisms authorized by
  the application host.
- `src/adapters/qt/` translates Qt events into semantic intents and applies
  projections incrementally. It also owns credential input, focus,
  accessibility, layout, and appearance.
- `src/app/` is the only composition root that selects concrete profiles,
  transports, vaults, workers, the provider, host, and Qt presentation.

See the [GitHub host implementation
architecture](docs/architecture/github-host-implementation.md) and
[normative system architecture](docs/architecture/system-architecture.md).

## Build and test

Prerequisites are CMake 4.4.2, Ninja, a C++23 compiler, Qt 6.8+ for the native
application, Python 3.10+ for tests, and Git 2.39+ for installed-Git contract
tests. Clang-format and clang-tidy are required for the coding-standard gates;
LLVM 22.1.8 is the validated tooling version. On Linux, libsecret 0.20+
development metadata is also required.
Compatible newer installations are supported.

Dependency discovery is centralized before product targets are declared.
Caller-installed Qt is resolved in package mode and is never fetched;
Python, Git, and Linux libsecret use their standard CMake or `pkg-config`
providers only when their selected graph needs them. SQLite 3.53.4 is
source-acquired through a verified, download-only CPM 0.43.1 provider and
built from a controlled two-file staging directory as the existing
`BetterPullReview::SQLite3` static target. Set standard hints such as
`Qt6_ROOT`, `Python3_ROOT_DIR`, `Git_ROOT`, `CMAKE_PREFIX_PATH`,
`PKG_CONFIG_PATH`, and `CPM_SOURCE_CACHE` when discovery needs help.
Until the install/export work in RFC-BPR-0014, configure BetterPullReview as
the top-level project. Top-level dependency-provider hooks are rejected, and
repository plus built-in CMake modules are loaded from trusted absolute paths.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

The same configure/build/test sequence is available as one workflow:

```sh
cmake --workflow --preset dev
```

Build and test the semantic core and application host without Qt:

```sh
cmake --preset core-only
cmake --build --preset core-only
ctest --preset core-only --output-on-failure
```

The canonical first-party C++ rules are in
[`_coding-standard/STANDARD.md`](_coding-standard/STANDARD.md). Check the
complete source and test tree with:

```sh
cmake --build --preset dev --target coding-format-check coding-tidy
```

`coding-format` applies the canonical formatter. The equivalent direct scripts
and project-specific clang-tidy adaptations are documented in
[`_coding-standard/README.md`](_coding-standard/README.md).

Use `-DBUILD_TESTING=OFF` for a tests-disabled graph. The obsolete
`BPR_BUILD_TESTS` cache variable is rejected with a migration diagnostic.
`-DBPR_DEPENDENCY_OFFLINE=ON` permits no CPM downloads and requires the pinned
CPM script and SQLite source to exist in the selected `CPM_SOURCE_CACHE`.

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
cannot create a profile, and query-bearing redirects fail closed. The
application prompts for native-HTTPS credential material only after the user
activates that profile.

Detailed commands, targets, runtime audits, and evidence expectations are in
[Build and Test](docs/development/build-and-test.md).

Create a single upload-ready Markdown snapshot of the repository source with:

```sh
python3 scripts/make_project_source_dumps.py
```

The generator writes
`artifacts/better_pull_review_project_source.md`, excluding build output,
caches, generated artifacts, and binary files. Use `--help` for alternate
output, standard-output, and generated-file options.

All configure, build, test, package, release, and verification work is local
or target-native. GitHub Actions has zero role.

## Repository workflow

- [RFC index](docs/rfcs/README.md)
- [Issue board](docs/issues/README.md)
- [Contributing](CONTRIBUTING.md)
- [Security baseline](docs/security/foundation-security.md)
- [RFC-BPR-0002 verification ledger](docs/verification/RFC-BPR-0002.md)
- [RFC-BPR-0003 verification ledger](docs/verification/RFC-BPR-0003.md)
