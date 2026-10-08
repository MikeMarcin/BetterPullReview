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
native HTTPS as the default production authentication transport,
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

RFC-BPR-0008 is Auto-Verified under the owner's r4 closeout amendment.
Its [ledger](docs/verification/RFC-BPR-0008.md) separates the implemented and
locally verified publication paths from pending live mixed-client/state-loss
checks (BPR-0143) and real whole-review attribution (BPR-0144). Those cards
remain parked follow-ups; RFC status does not claim those observations or
complete release qualification. The private-PR smoke (BPR-0641) was accepted by
the owner on 2026-09-25; the pull request opened read-only but showed no file
comparison, which is BPR-1199.

**Prepare review tour** opens private Problem and Solution Markdown fields and
an ordered route of source-bound explanations. Add stops from selected code,
reorder them with `Alt+Shift+Up/Down`, and use Previous, Next or Return to tour
while exploring files. Drafts save locally and survive reopening; editing and
preview require no AI and publish nothing. Import, publication and recovery are
implemented through production-port fixtures; production Tour writes remain
unavailable until exact host/transport qualification. See
[private review-tour authoring](docs/architecture/review-tours.md).

Ordinary review threads can now be
classified into private durable defect cards: evidence remains distinct from
explicit verification, the stable multi-defect queue survives restart, and a
separate Publish action is still the only shared path.

The **Private history** panel keeps review threads, agent runs, and composition
sources in the local profile ledger. **Squash to Draft** saves a local draft;
publication remains a separate action in the draft editor. The
[private review workflow](docs/ux/private-review-workflow.md) explains actor and
model qualifications, stale anchors, split targets, deletion, and public text.
RFC-BPR-0023's [verification ledger](docs/verification/RFC-BPR-0023.md) records
local evidence and the pending designated GitHub readback.
RFC-BPR-0009 is Auto-Verified under the owner's r4 closeout amendment. Its
[ledger](docs/verification/RFC-BPR-0009.md) records the verified local defect
workflow while BPR-0153 remains parked for an unfamiliar GitHub collaborator's
comprehension and reply. No collaborator verdict is inferred from RFC status.

The shipped workbench now offers explicit built-in agent profiles and carries
one reviewer gesture through core host policy, exact accepted context, an
isolated exact-head workspace, the real process adapter, and visible terminal
projection. Windows uses the native suspended-start/Job Object backend; its
component evidence does not qualify the RFC-0014 installed-package matrix.

RFC-BPR-0011 is Auto-Verified under the owner's r4 closeout amendment. Its
[ledger](docs/verification/RFC-BPR-0011.md) binds the verified local fixture
and public invocation paths plus candidate-specific historical native evidence.
Installed-agent compatibility, actual agent GitHub authentication and human
judgment of the existing-checkout warning remain unobserved; no release or
native package acceptance follows from this status.

RFC-BPR-0012 is Auto-Verified under the owner's r4 closeout amendment. Its
[ledger](docs/verification/RFC-BPR-0012.md) records the verified local findings,
workshop, revision-aware chat and read-only MCP paths. The later Dif workshop
cue mismatch remains a separate BPR-0966 follow-up. Installed-agent behavior,
live agent GitHub identity and native package qualification remain unobserved;
the status preserves the original fixture acceptance scope.

The shipped landing surface now accepts local repository, author, and path
observer rules, persists the exact matched reason, restores it across a restart,
and removes the projected item when its only local rule is deleted. Explicit
role, observer-reason, attention-bucket, repository, and quiet/current filters
sit above the core order; repository and pull-request-number sorts activate only
when selected, and the private preferences survive restart without persisting a
derived filtered queue.

RFC-BPR-0010 is Auto-Verified under the owner's r4 closeout amendment. Its
[ledger](docs/verification/RFC-BPR-0010.md) records the verified local queue and
notification composition. BPR-0169 remains parked for an independent live
GitHub state comparison; BPR-0168 retains the accepted v006 native checkpoint
and the later-candidate replay obligation as a standalone backlog follow-up.
RFC status supplies neither a new server observation nor a new native verdict.

RFC-BPR-0015's amended first read milestone has a qualified macOS fixture
path. Native HTTPS remains the default. The tests-disabled macOS composition
has explicit
CLI read opt-in on the selected GitHub.com or GHES host, with a Connect warning,
observed `/user` identity, pre/post-request account checks and a read-only lease.
It invokes the configured `gh` directly with bounded arguments, input, output
and time; account discovery and API use require `gh` 2.x with minor >= 96
because the account inventory uses `gh auth status --active --json hosts`.
Older versions are refused before account status runs and the UI requires an
upgrade. BetterPullReview never passes `--show-token`.
It uses `gh`'s default home-directory configuration and ignores custom CLI
environment overrides.
This is a version check, not proof of an official executable's provenance.
The r2 trust-boundary amendment permits a bounded exact-repository `gh` Git
operation for objects; `gh` may internally launch Git and its shell credential
helper. Every macOS `gh` launch must run under OS-enforced file-write denial,
with a narrowly owned temporary object-store exception only for the bounded
clone; an unavailable boundary refuses the CLI profile. This covers `gh`'s
possible first-use global configuration migration, which a post-request check
cannot prevent. BetterPullReview must not read the helper's token exchange or
change global authentication. Full Git comparison requires `gh` 2.96 or later
and Git 2.49 or later, not one exact vendor or banner. Bounded, write-denied
local help probes must show the `gh` and `git clone` options the app uses before
any credentialed call; a failed check and an unsupported tool are reported
separately. Capability admission retains actual clone and exact-object validation. Native
qualification evidence remains installation-specific. The 32 MiB per-file
ceiling and supervised 96 MiB/128-entry temporary-store abort threshold remain; the
aggregate threshold is not a hard disk quota. Local refs reflect the accepted
API snapshot, not fresh remote advertisement. A tests-disabled macOS fixture
now renders a full PR diff through the CLI profile; an isolated build with
the process adapter removed from composition visibly refuses the same healthy
fixture. A macOS native read-only probe now verifies one official `gh` HTTPS clone of a
small owner-controlled private repository through `gh auth git-credential`;
`gh` reports a keyring token source and a matching Keychain item exists.
The probe does not bind the precise Security API calls, credential generation,
or concurrent account selection. The private repository name and objects
were not retained. On macOS, the opted-in CLI profile now routes whole-review and
right-side inline-comment publication through the existing mutation admission,
a separate exact-host/account/PR/operation confirmation for each ordinary write, and
server reconciliation. The configured gh account may still change between
checks; an ambiguous attempted write is not posted twice. Fixture actions
exercise this path; no live PR was written. Windows and Linux remain
unavailable. Mac qualification is recorded by BPR-0787. RFC-BPR-0014 is now
Auto-Verified, and RFC-BPR-0015 became Auto-Verified on 2026-09-23 through
BPR-0788. The product owner approved the pending review of the CLI
authority and protocol changes made after that qualification. That
approval is an owner decision, not an independent reviewer's report.

Tour Publish is a bounded grouped-confirmation extension: the preview binds one
immutable plan to the exact actor, head, sources, prose and allowed returned-link
slots. Each journaled write consumes its own one-use child grant. Stop or an
identity/draft change revokes remaining authority; restart reconciles read-only
and needs an explicit preview and Resume. Compatibility comments are explicitly
non-atomic, and an optional description edit is a separately named operation.
These Tour operations remain unavailable on an unqualified host/transport.

After a CLI profile connects, pasting a PR link for that exact host reuses the
accepted CLI session, including when an unconnected native profile also exists
for that server. Home and pasted PR links share a focused sign-in dialog;
Settings manages existing connections. Use GitHub CLI starts discovery immediately:
a single matching account advances to the trust confirmation, while multiple
accounts offer a choice. Acceptance resumes the pending PR for that exact host.
Cancellation discards the pending setup and ignores late discovery results. Home does
not yet discover authored or requested PRs automatically; personal discovery
remains tracked under RFC-BPR-0026.

BetterPullReview does not extract or copy CLI tokens or change global CLI
authentication. Opt-in trusts the machine's configured executable, configuration,
socket routing, CA and redirect behavior, active account and helper processes.
Pre/post observations can detect some account changes but cannot bind a credential
generation or prevent a concurrent account switch. Executable/configuration/CA
replacement and process escape remain uncontained risks. Read opt-in grants no
publishing authority. RFC-BPR-0013 keeps a 250-record limit for commits returned by the pull-request
endpoint. A larger reported total is accepted only when that endpoint
returns exactly 250 records ending at the accepted head. The UI names the
reported total and returned count separately; this does not establish that
any particular private pull request can render.

RFC-BPR-0013 is Auto-Verified under owner amendment r4 for its documented bounded
hardening scope. The [closeout ledger](docs/verification/RFC-BPR-0013.md) preserves
the original wider requirements as BPR-0967–0972 follow-ups, including current
Mac memory-budget failures, broader native performance, rendered offline/backoff,
fuzzing and diagnostic/structural coverage. Existing human, layout and tidy
follow-ups remain open; this status does not qualify a release.

Provider “modified” metadata can still disagree with the selected base-tip
comparison. A missing required side makes the comparison unavailable.
BPR-0944 retains preserving the readable side, explaining the missing side in
its pane, and deciding comparison semantics; that behavior is not implemented.

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

The current workbench composes the accepted ten-clip Dif catalog from
RFC-BPR-0030/0031. One decorative sprite rests in the status bar's 28-pixel
leading corner, moving to the first-time Home or Compose workshop prime spot
when that surface is presented. Profile-owned visibility
and motion preferences apply through the existing Appearance controls; reduced
motion selects static `rest`, and a refused catalog leaves the Home and review
controls functional without art. Dif is decorative and consumes only accepted
presentation facts. Both RFCs are `Auto-Verified` after the focused native,
rendered, memory, resource, and core-only closeout. The selected-graph evidence
does not certify a signed, notarized, or installable package. RFC-BPR-0020's
[foundation closeout](docs/verification/RFC-BPR-0020.md) reconciles its original
criteria with those accepted successor requirements and placement decisions.

## Review tours

Use **Prepare review tour** in Overview or the Pull Request menu to write a
Problem, Solution and ordered source explanations without AI. The
[operator guide](docs/development/review-tours.md) covers native editing, optional
writing hooks, cold import and partial maintenance. Production Tour writes remain
unavailable until exact server/transport qualification; the
[execution ledger](docs/verification/RFC-BPR-0039-0043.md) distinguishes tested
composition from unobserved native, website and installed-model qualification.
RFC-BPR-0039–0043 are Auto-Verified under the owner's
[r2 implementation-scope amendment](docs/verification/RFC-BPR-0039-0043.owner-closeout-r2.md).
Standalone Blocked BPR-1222–1226 retain GitHub.com/GHES live operations and website
courses, remaining native stations, installed-provider/model evidence and original
predecessor acceptance. RFC status enables no unavailable operation.

## Review Cycles

Use **File > Open Project…** or Home's Review Cycles section to review a folder,
a file or a Git checkout of your own, with no account, remote, commit or agent.
Write notes and exact suggested edits on any line, compose them into one frozen
round, and hand it over with **Copy**, **Save** or a bundle. After the changes,
**Capture revision** reads the folder again, the comparisons show what changed,
and you record replies and your own verification before the next round. A
finished result can become a new pull request on GitHub.com
(**Publish as Pull Request…**). `bpr cycle` works the same cycles from a
terminal, and a harness that runs `bpr` can read one round you grant it. The
[Review Cycles workflow](docs/ux/review-cycles-workflow.md) is the guide and the
[terminal guide](docs/development/cycle-cli.md) the command reference.
RFC-BPR-0053–0061 stay Proposed until their closeout cards; the
[execution ledger](docs/verification/RFC-BPR-0053-0061.md) records what is
qualified on which platform and what is still owed.

## Installation, privacy and release status

The [package guide](docs/development/release-packages.md) covers per-user
installation, declared platform/toolchain baselines, dependency notices,
separate corresponding-source assets and independent signature checks.
RFC-BPR-0014 is Auto-Verified for its owner-amended packaging/maintenance
foundation, completing the original RFC pack. Full release qualification remains pending under
[RFC-BPR-0038](docs/rfcs/RFC-BPR-0038-native-package-release-security-qualification.md):
current macOS/Windows candidates
lack final release trust, dependency vulnerability findings remain unresolved,
and the complete native install/authentication/vault/removal matrix is open.
Package definitions and a universal binary do not establish that acceptance.

Use Settings → Storage & privacy to preview and export bounded diagnostic
metadata or prepare explicit diagnostic cleanup/profile purge. Preparation
stops that profile; cancellation does not restart it. Uninstalling program
files retains private state. The [storage and removal guide](docs/development/storage-and-diagnostics.md)
names included data, credentials, recovery and residuals. The
[corporate security overview](docs/security/corporate-security-overview.md)
describes actual network destinations, subprocesses, AI and privacy boundaries,
with current qualification gaps.

## Application updates

The [Settings guide](docs/development/settings.md) describes all seven
categories, their defaults, persistence, reset scopes and platform capabilities.

On eligible production-signed macOS installations, **Check for Updates…** and
General Settings share application-wide update preferences. Select native
GitHub.com release access explicitly, then choose Download and Install and Restart
for the verified version. Managed installations and other platforms use manual
distribution. Packages predating the helper protocol need a manual bootstrap.
[Application updates and recovery](docs/development/application-updates.md)
explains the workflow and retained data. Apple Silicon disposable signed upgrades
are verified; Intel validation is deferred to nonblocking BPR-0950, while live
private-release/vault evidence remains required in the
[RFC-0035 ledger](docs/verification/RFC-BPR-0035.md).

## Source-line comments

The ordinary line comment gesture includes materialized current/right lines
outside a hunk in a text file changed by the pull request. Native publication
uses the exact current head and acting identity. The implemented file-comment
compatibility path retains a visible code permalink and canonical GitHub IDs,
with fallback admitted only for the owner-approved exact PR2/source/actor/profile
contract. A signed app publication and subsequent GitHub-only reply have been
verified; broader native success and installed-removal acceptance remain unobserved.
RFC-BPR-0022 is Auto-Verified under the owner’s r4 evidence amendments.
See [source-line comments](docs/development/source-line-comments.md) for behavior
and current evidence limits.

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
libsecret 0.20+ and libxcb development metadata are also required.
Compatible newer installations are supported.

Dependency discovery is centralized before product targets are declared.
Qt 6.11.2 is acquired by a pinned provider and resolved in package mode,
with `BPR_QT_ROOT` and `BPR_QT_PROVIDER=OFF` available to supply it yourself;
Python, Git, and Linux libsecret and libxcb use their standard CMake or
`pkg-config` providers only when their selected graph needs them. SQLite 3.53.4 is
source-acquired through a verified, download-only CPM 0.43.1 provider and
built from a controlled two-file staging directory as the existing
`BetterPullReview::SQLite3` static target. Set standard hints such as
`Qt6_ROOT`, `Python3_ROOT_DIR`, `Git_ROOT`, `CMAKE_PREFIX_PATH`,
`PKG_CONFIG_PATH`, and `CPM_SOURCE_CACHE` when discovery needs help.
Configure BetterPullReview as the top-level project. Top-level dependency-provider
hooks are rejected, and repository plus built-in CMake modules are loaded from
trusted absolute paths. RFC-BPR-0014 owns the application installation/package
foundation; RFC-BPR-0038 owns remaining native and release-security qualification.
Neither requires a separate core SDK or general CMake embedding
support. The core remains independently buildable and testable without Qt/providers.

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
