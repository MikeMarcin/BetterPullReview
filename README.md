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

On macOS, select a GitHub.com native HTTPS profile and click **Public preview**
beside Connect to open public pull requests without signing in. It shows metadata, changed-file diffs, and REST inline comment
groups. It has no viewer identity, review-thread resolution, viewed-file state,
triage, or publishing. The UI offers Sign in for those capabilities. Anonymous
requests share the server-reported hourly budget; a 404 means private or missing
without distinguishing them. Linux and Windows currently report preview
unavailable, and GHES preview is refused. The
[RFC-0017 ledger](docs/verification/RFC-BPR-0017.md) separates this implemented
Mac path from outstanding RFC closeout work.

The final-state review in BPR-0225 narrowed the workbench claim. BPR-0226 now
connects the shipped RFC-BPR-0005 shell's primary intents and drawer widths to
the workbench host, publishes host state to the real window, observes exact
accepted pull-request authority, clears it by identity, and retains a
session/default settings host when SQLite cannot open. BPR-0227 now carries a
real file-drawer selection through the semantic workbench intent, resolves its
exact current/previous repository paths independently at each selected commit
on the epoch host's retained cache session, and publishes the
identity-complete comparison through the existing diff host. Accepted epoch
comparison changes replay the retained file selection, while an authority
refresh clears the old rows before replacement work begins.
RFC-BPR-0006 through RFC-BPR-0011 have substantial tested component
implementations. The shipped workbench now composes its review-ledger host on
the exact accepted host/account/repository/pull-request authority: selected-file
receipt actions persist before projection, exact material receipts inform
actionability, and notes, reviewed heads, and defect records share that host.
Theme customization/import is also composed. The shipped draft editor now
binds the diff workbench's exact semantic selection and resolved current-side
repository path to an asynchronous review-mutation host. The fixture path
drives the real Publish action through core policy, host admission, the
coordinator, server refresh, and typed terminal presentation; stale targets are
refused and lost results are retried idempotently. Selection changes during an
in-flight publication retain the latest emitted selection until its matching
terminal has reconciled server truth, including a return from A to B to A, and
the epoch-sealed right endpoint cannot be replaced by an arbitrary path. The tests-disabled production application now composes inline publishing for
its exact accepted authenticated PR on GitHub.com or an explicitly configured
GHES host. Each ordinary Publish gesture supplies a short-lived one-use grant
bound to the complete draft, selected diff, session, target and acting account.
Whole-review Comment, Approve and Request Changes use their separate normal
Submit review workflow. Test builds cannot issue normal production grants.
The designated attended validation launcher retains its separate exact-target,
one-shot approval course; ordinary use needs no launcher or approval artifact.
The host
propagates per-request cancellation through live observation, submission, and
native HTTPS, revokes admission on identity replacement and shutdown, and durably
reconciles an indeterminate attempted publication from server truth after
restart without a second POST.
Ordinary review threads can now be
classified into private durable defect cards: evidence remains distinct from
explicit verification, the stable multi-defect queue survives restart, and a
separate Publish action is still the only shared path. The shipped workbench now
offers explicit built-in agent profiles and carries one reviewer gesture through
core host policy, exact accepted context, an isolated exact-head workspace, the
real process adapter, and visible terminal projection. Windows launch remains
fail-closed pending target-native containment evidence.
The shipped landing surface now accepts local repository, author, and path
observer rules, persists the exact matched reason, restores it across a restart,
and removes the projected item when its only local rule is deleted. Explicit
role, observer-reason, attention-bucket, repository, and quiet/current filters
sit above the core order; repository and pull-request-number sorts activate only
when selected, and the private preferences survive restart without persisting a
derived filtered queue.

GitHub CLI choices remain visible but deliberately unavailable: their trust
contract is outside milestone-one product scope, so selecting one returns the
fixed typed `cli_unavailable` result with zero CLI executable/configuration
discovery or CLI process launch. Positive CLI enablement is deferred to
[RFC-BPR-0015](docs/rfcs/RFC-BPR-0015-github-cli-production-enablement.md).
Manual credentials use a Qt-owned password input and remain session-only.
Device sign-in is remembered across orderly quits: tokens stay in the platform
vault, while a private metadata store retains the exact binding and lifetime.
Startup revalidates the binding, token and account before accepting a session;
expired or mismatched material requires sign-in. **Sign out** removes both
token slots and disables the stored binding. Replacing a device grant with a
manual token removes the prior grant. The Git history path retains only revisions BetterPullReview actually
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

The shipped queue keeps the first accepted head as an exact session/review
baseline and emits no activity for it. A later accepted head that differs from
both that baseline and the accepted current-viewer review can produce one
metadata-only notification request. Because `QSystemTrayIcon` supplies no
per-message activation identity, the process-lifetime sink submits at most the
first actionable payload and suppresses every later one; its first click can
therefore select only that immutable exact queue row. A filtered or departed
target reports a local failure without changing filters or opening provider
navigation. Whether each desktop actually shows the native banner remains an
attended permission/platform gate.
The shipped window is `workbench_window`, not the deleted
`github_window`; it presents the compact pull-request shell, request states,
retained accepted content, read-only conversations, and connected epoch
controls. The natural-layout selected-file comparison, workbench navigation,
drawers, and detail components are reachable through the shipped shell and a
real gesture-driven executable contract. Appearance profiles and syntax
components remain composed
when their durable settings store cannot open. The window then starts from
System defaults, labels appearance changes as session-only, and keeps syntax
classification and profile switching available; a restart under the same
failure returns to those defaults. Done BPR-0237 adds semantic-role preview,
cancel, persistence, restart restoration, and bounded safe theme import against
the selected comparison. BPR-0233 owns its recorded selected-diff pixels.

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

Prerequisites are CMake 4.4.2, Ninja, a C++23 compiler, Qt 6.8+ with CoreTools
for the native application, Python 3.10+ for tests, and Git 2.39+ for
installed-Git contract tests. Clang-format and clang-tidy are required for the
coding-standard gates; LLVM 22.1.8 is the validated tooling version. On Linux,
libsecret 0.20+ development metadata is also required.
Compatible newer installations are supported.

Dependency discovery is centralized before product targets are declared.
Qt 6.10.3 is acquired by a pinned provider and resolved in package mode,
with `BPR_QT_ROOT` and `BPR_QT_PROVIDER=OFF` available to supply it yourself;
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
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review
```

Use `--bpr-profile ID` to select a stable local BPR profile identity and
`--new-window` to request another window in that profile. Ordinary repeated
launches activate its most recent window through the existing owner on supported
POSIX roots. A routed process exits without creating a competing runtime.
Refused or indeterminate handoffs report a fixed diagnostic; they do not retry
New Window automatically. Windows activation transport and roots whose socket
path exceeds the native address bound remain unavailable.

On macOS both graphs build application bundles, which is why the path is nested;
on Linux and Windows the executable is flat at
`build/dev/src/app/better_pull_review`. See
[the bundle note](docs/development/build-and-test.md) for why.

Add an explicit GHES 3.18 hostname when needed:

```sh
build/dev/src/app/better_pull_review.app/Contents/MacOS/better_pull_review --ghes-host ghe.example.com
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
