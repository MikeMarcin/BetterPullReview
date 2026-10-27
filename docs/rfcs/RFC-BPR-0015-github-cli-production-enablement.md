---
id: "RFC-BPR-0015"
title: "Opt-in GitHub CLI Account Profile"
status: "Auto-Verified"
area: "BPR"
milestone: "Post-Pack v2"
depends_on: ["RFC-BPR-0014"]
pack_version: "post-v2"
metadata: {"milestone_plan":{"id":"rfc-bpr-0015-cli-profile","path":"docs/rfcs/plans/rfc-bpr-0015-cli-profile/milestone-plan.json"}}
---

# RFC-BPR-0015: Opt-in GitHub CLI Account Profile

## Owner Windows and Linux enablement amendment — 2026-10-27

The owner wrote, in chat on 2026-10-27, "Why do I not have the use GitHub CLI
option on Windows when I first launch it? That should be there", and then "It
should work for Linux too." That decision replaces the macOS-only production
enablement below ("Windows and Linux remain unavailable, composing no GitHub
CLI entry") for Windows and Linux. [[BPR-1791]] implements it.

- **Same gestures and gates.** Home, the sign-in surface and Accounts & hosts
  offer Use GitHub CLI on Windows and Linux under the macOS rules: only that
  gesture runs `gh` before a profile exists, a run inside the automated test
  boundary withholds it ([[BPR-0797]]), the read question and remembered
  choice are unchanged, every call is pinned to the chosen account through a
  private configuration, each operation acquires its own strict credential
  (RFC-BPR-0078), and every existing failure state is kept. One build switch,
  `BPR_GITHUB_CLI_PLATFORM`, names the three platforms.
- **Finding `gh`.** `PATH` first, then the platform's standard locations,
  each canonicalised, taking the first that is really `gh`/`gh.exe`. Windows:
  `%ProgramFiles%\GitHub CLI`, `%LOCALAPPDATA%\Programs\GitHub CLI`, WinGet's
  per-user links and Scoop's current `gh`. Linux: `/usr/bin`,
  `/usr/local/bin`, `/home/linuxbrew/.linuxbrew/bin`, `~/.local/bin` and
  `/snap/bin`. The snap launcher canonicalises to `/usr/bin/snap` and is not
  taken. The child's environment comes from the shell's known folders on
  Windows and from the home, `XDG_RUNTIME_DIR` and its session bus socket on
  Linux.
- **Disclosed differences.** No operating system denies `gh` file writes on
  Windows or Linux, so the unpinned version and inventory calls of an explicit
  gesture can let `gh` migrate its global configuration; pinned calls read only
  their private configuration. The Windows private configuration uses the
  shared owner-private descriptor with read-only files, but its directory is
  not write-locked as the POSIX one is. Git-object acquisition and exact blob
  reads through `gh` stay macOS-only, so full diffs through GitHub CLI are
  unavailable there and the read warning says so. A Linux session without a
  bus socket fails configuration and is reported rather than waiting on a
  keyring.
- **Unchanged.** Fixed argument arrays, no shell, no ambient `PATH` in the
  child (Windows passes an empty one), redirect and account-switch handling,
  redacted failures and no logged bodies. The redirect, CA, executable,
  keyring-helper and process residual risks recorded below apply on all three
  platforms. Historical criteria, closeout and plans keep their macOS scope;
  the last criterion's "unsupported platforms" now means platforms other than
  macOS, Windows and Linux. Native Windows and Linux evidence is BPR-1791's.

## Owner strict-credential amendment — 2026-10-13

The adopted `strict-cli-publication-r2` mandate and
[RFC-BPR-0078](RFC-BPR-0078-strict-cli-account-credentials.md) supersede the
historical blanket tokenless rule for the chosen macOS CLI account only. A closed
stock `auth token --hostname HOST --user LOGIN` lookup under write denial/private
token-free configuration supplies one bounded adapter-owned transient credential.
It reaches only same-operation identity checks, API and gh-owned Git/helper
children through the dedicated sensitive process path. No parent token,
cross-operation cache, persistent BPR copy, global account mutation, fork or direct
production Keychain reader is permitted. An unchosen account or failed named
lookup refuses before network work; missing and inaccessible lookup share
`credential_unavailable`. Tool failures and attempted-write uncertainty remain
separate. Numeric account/login validation, exact mutation admission,
GET-only recovery, process confinement and current platform eligibility remain.

Stock-gh native qualification and memory/persistence limits are recorded in
[the strict ledger](../verification/RFC-BPR-0078.md). Source-owned buffers are
cleared only after queued work and descendants drain; Qt/OS/stock-child copies,
swap/crash capture and process escape remain outside perfect-erasure claims.
[The companion](RFC-BPR-0079-remove-cli-publication-warning.md) owns removal of
the additional publication warning after strict qualification; that UI work is
not part of this amendment's evidence. Earlier amendments, imported plans,
qualification records and BPR-1373's pending owner judgment retain their exact
historical candidates and obligations.

## Product-owner amendment, 2026-09-13

The owner requests an account profile using an already configured official
`gh` installation for **both reading and publishing reviews/comments**. The
motivating case is a private repository readable by the owner's CLI credential
but not by the present GitHub App grant. This direction explicitly replaces
the original indefinite upstream deferral and blanket prohibition on partial
CLI mode below. It permits implementation before RFC-BPR-0014 closes; the
formal dependency and canonical closeout order remain. Native HTTPS stays the
default and no existing session is silently converted. The original deferred
contract remains below as historical risk analysis, not current acceptance.
No live pull-request mutation is authorized by this RFC or its implementation.

An account row offers `GitHub CLI (uses your gh sign-in)` for an exact
configured GitHub.com or GHES hostname. Before explicit opt-in it states that
BetterPullReview will run the user's existing `gh`, that its account and
permissions can differ from an App/native profile, and that CLI executable,
configuration, certificate and redirect behavior is trusted as configured on
this machine. The observed `/user` login and selected host are shown. Read
opt-in does not authorize publishing. A separate confirmation immediately
before each CLI-backed publication names the host, observed login, repository,
pull request and kind of write, and acknowledges the account-switch risk.

The current `gh api` has no per-invocation account selector. A concurrent
`gh auth switch` or credential/configuration change can occur between identity
observation and a request, including during pagination or publication. Rechecking
identity detects some changes but cannot prevent that race. Neither warnings
nor pre/post checks may be described as exact account or credential-generation
containment. An observed mismatch stops the session or write; an ambiguous
attempted write must reconcile server truth without issuing a second POST.

The narrower enforceable connector boundary is:

- Account discovery and API use require `gh` 2.96 or newer so the app can use
  the structured active JSON inventory. Older versions are refused before
  account status runs and require an explicit upgrade. Human auth-status text
  and stderr do not enter the process result or typed account boundary, and
  the app never passes `--show-token`. This compatibility floor does not
  weaken the separate exact 2.100.0/2.101.0 qualification for Git-object
  cloning.
- Only the explicit Use GitHub CLI gesture (for the bounded version and
  active-account inventory requests) or an explicitly selected CLI profile may
  execute `gh`; native profiles never use it. Resolve an absolute official
  executable, record its version and observed identity, and refuse detected
  changes. Path preflight cannot prove the exact image loaded against
  concurrent replacement.
- Invoke directly with an argument array, never a shell. Construct a narrow
  noninteractive child environment, choose the exact HTTPS hostname, use a
  neutral non-repository directory and bound input/output/time/cancellation.
  Do not extract, print, pass, log, persist or copy the CLI token; do not run
  `gh auth switch`, `gh auth token` or a local listener.
- Git-object acquisition may invoke the selected official `gh` to perform an
  exact-repository Git operation. That `gh` invocation may launch Git and its
  own `gh auth git-credential` shell helper. BetterPullReview does not invoke
  a shell or helper directly, inspect the helper exchange, configure a global
  Git helper, or alter `gh` account selection. On macOS, every `gh` invocation,
  including version and REST probes, must launch through an OS-enforced
  file-write denial with only a narrowly owned temporary object-store exception
  for the bounded Git operation. Refuse the profile if that launch boundary is
  unavailable. A post-request configuration observation alone is insufficient:
  `gh` can migrate old configuration during its first invocation. The child
  Git process and helper
  are additional trusted executable/configuration surfaces, not an exact
  credential or account containment guarantee. Verify the observed host and
  login before and after acquisition, refuse detected changes, bound the
  operation and its temporary data, and retain the documented race between
  those observations. A missing or failed Git-object path leaves the full diff
  unavailable rather than silently using native credentials.
- Once `gh` has acquired the exact repository objects into a bounded temporary
  store, BetterPullReview may launch credential-free local Git with closed
  arguments to serve those objects through the existing Git protocol v2 pack
  pipeline. The local operation must not perform a remote fetch, invoke a
  credential helper or hook, or accept repository-controlled executable
  configuration. Bound temporary-disk use, output, time and cancellation;
  remove the store on all terminal paths. This local Git subprocess does not
  provide stronger identity containment for the earlier authenticated clone.
- The macOS Git-object qualification is deliberately exact: `gh` 2.100.0 or
  the directly checked 2.101.0, and Apple Git 2.50.1 (Apple Git-155), a 32 MiB
  per-file ceiling and a supervised 96 MiB/128-entry temporary-store abort
  threshold. The
  aggregate threshold is not an OS disk quota; an in-flight write can exceed
  it before observation. Refuse an unsupported version or over-limit pack
  without silently falling back to native credentials. The refs supplied to
  the existing protocol parser describe the accepted API snapshot and do not
  prove fresh remote Git refs; retain the existing revision/object checks.
- Parse actual bounded CLI status, headers and body into the existing provider
  response contract. Do not infer HTTP success from process exit alone. Keep
  failures fixed and redacted, and do not log request bodies or private data.
- Preserve the existing public read, pagination, review-mutation admission,
  target identity and indeterminate-publication reconciliation contracts.
  Fixture or owner-designated safe-test-repository publication is the only
  validation authorized here.
- Initial production enablement is macOS-only. Windows and Linux remain
  unavailable, composing no GitHub CLI entry, pending target-native connector,
  credential, process, read/write and UI evidence. Test fakes cannot enable
  production support.

The original BPR-0037 investigation showed that official `gh` 2.95/2.96
followed a cross-authority 302, honored ambient `http_unix_socket`, and sent
an authenticated request through an AF_UNIX sentinel; the macOS keyring path
launched `/usr/bin/security`. Executable, config, CA, current-directory and
process objects remain raceable without stronger upstream facilities. These
are **residual risks of the explicit CLI trust choice**, not passing safety
tests. The account UI, publication confirmation and threat documentation must
name redirect, socket/config/CA, account-switch and process risks plainly.

CLI authority alone does not make `EpicGames/UnrealEngine#1773` render. The
separate RFC-BPR-0013 parser currently rejects its reported 9,311 commits at
`MaximumCommits=250` with `response_limit`, despite CLI HTTP 200 on the first
two requests. This RFC does not change that ceiling.

## Closeout reconciliation r3, 2026-09-22

This records how the as-built product differs from the amendment's wording
above. It changes no acceptance criterion below and supplies no evidence.

- [[BPR-0909]], [[BPR-0913]], [[BPR-0914]] and [[BPR-0915]] (owner directed,
  2026-09-18) made GitHub CLI a discovered way in. Use GitHub CLI, on Home,
  in the shared sign-in dialog and on Accounts & hosts, is the only gesture
  that runs `gh` before a profile exists: `gh --version` and
  `gh auth status --active --json hosts` under the same write-denied
  boundary, never `--show-token`, never on merely opening a surface. Each
  admitted exact active host/login becomes a CLI profile; a GHES host needs no
  pre-added account and gets a CLI-only card naming the observed login. A
  pasted PR link for that exact host reuses the accepted CLI session. Every
  read still passes the read opt-in, and every write its own confirmation.
- No card carries a precomposed `GitHub CLI (uses your gh sign-in)` or
  `GitHub CLI (unavailable)` row any more. Windows and Linux therefore show no
  CLI entry at all rather than a visibly unavailable row; they still cannot
  run `gh`.
- eecdda2a added the directly checked `gh` 2.101.0 to the Git-object
  allowlist beside 2.100.0; Apple Git and the size bounds are unchanged.
- [[BPR-0933]] shortened the read opt-in: a headline question and a short
  consequence paragraph, with every residual risk above behind Show Details.

## Closeout, 2026-09-23

**Auto-Verified for the qualified macOS scope.** Closeout
[[BPR-0788]] records direct evidence for all six criteria below, rechecked at
`c1a336a0` in round r4, in the
[verification ledger](../verification/RFC-BPR-0015.md#closeout-round-r4-2026-09-23).
RFC-BPR-0014 became Auto-Verified first.

Owner amendment r4: on 2026-09-23 the product owner wrote, verbatim, in the
chat coordinating the closeout, "I also approve the BPR-0788 review". That
approval satisfies the pending authority and protocol review of the CLI
changes made after the d5 candidate `26f1c22e`. It is an owner decision, not
an independent reviewer's report. No criterion changed.

Windows and Linux still compose no CLI entry. Live pull-request publication
remains unauthorized, and no live write was made. The residual executable,
configuration, CA, redirect, account-switch and process risks recorded above
are disclosed, not contained. [[BPR-0803]] (Use GitHub CLI during a device
flow) remains separate Backlog work.

## Acceptance criteria

- The opt-in account row names host, observed login, configured-CLI trust risks and read-versus-publish choice without silently replacing a native profile or claiming exact account containment.
- A tests-disabled macOS application connects through configured `gh` and renders a representative fixture PR via the real public path; missing or changed executable, host, account or config produces a typed refusal without credential fallback.
- CLI reads preserve actual status, headers, body, correlation, bounded pagination, cancellation and redacted errors; removing the process or response adapter makes a direct public-path negative control fail.
- Fixture review and inline-comment publishing traverse ordinary draft and Submit review admission, explicit CLI write confirmation, exact target and observed-account checks, bounded transport, reconciliation and typed terminal projection; a read opt-in or ambiguous write cannot issue a second POST.
- Token and private response content remain absent from parent arguments, environment, logs, diagnostics, settings, SQLite and committed evidence; the app does not change global `gh` authentication or configuration.
- Target-native qualification records supported platforms and residual executable/config/CA/redirect/account/process risks; unsupported platforms remain unavailable and canonical closeout awaits RFC-BPR-0014 and all evidence.

## Original deferred contract (superseded by the amendment above)

## Status and scope

This is a deferred, upstream-dependent RFC for work after the canonical
fourteen-RFC pack. It is not part of pack v2, does not extend that pack's
implementation order, and does not block RFC-BPR-0002 or any other pack-v2
RFC. RFC-BPR-0002 ships native HTTPS as its production authentication
transport; every GitHub CLI choice remains visibly fixed at zero-launch
`cli_unavailable`.

This RFC cannot move out of `Deferred` merely because a wrapper appears
plausible. The required guarantees must exist in an official GitHub CLI
release and must be directly provable through the exact installed program. If
the upstream contract never becomes sufficient, CLI mode remains unavailable
indefinitely.

## Motivation

An installed `gh` process can inherit authority from executable bytes,
configuration, credential storage, certificate configuration, its current
directory, and process topology. A production broker is safe only if
BetterPullReview can bind all of those inputs to the reviewed invocation and
reject changes before they affect a request. Detection after a credential or
body has reached the wrong endpoint is not containment.

The current official CLI does not provide the complete contract. In
particular, a production design must not depend on mutable active-account
configuration, token extraction, post-request identity checks, post-hoc
filesystem observation, or assumptions about redirect and child-process
behavior.

## Eligibility gates

Implementation may begin only after an official GitHub CLI release provides
all of the following:

- **Strict redirect preflight:** the caller can reject a redirect before it is
  followed. An authority other than the exact configured scheme, host, and
  port must be rejected before credentials or a request body can be
  forwarded. Every query-bearing redirect must fail closed, including one
  within the configured authority.
- **Per-invocation account binding:** one invocation can select the exact
  hostname and login without mutating or consulting a race-prone global active
  account and without falling back to an unkeyed host credential.
- **Correlated credential generation:** the selected credential remains the
  same across the identity check, pagination, retries, and permitted redirect
  processing for that invocation. BetterPullReview neither receives nor
  extracts the token.
- **Containable macOS authentication:** the official CLI can use the selected
  credential on macOS without launching `security`, a shell, or any other
  descendant. The broker can enforce a no-descendant process boundary rather
  than infer compliance after execution.

Upstream documentation alone is insufficient. Tests must exercise hostile
configuration, account switching, redirects, path replacement, and
cancellation against the exact supported CLI release.

## Required design

The application host owns a narrow GitHub CLI capability. The core sees only
typed requests, bounded observations, completions, and structured diagnostics;
Qt sees only projections. CLI provider types, paths, processes, and
credentials never cross inward.

Before allowing one launch, the host must establish a fail-closed invocation
that binds:

- the reviewed official `gh` version, digest, source revision, signing
  identity where applicable, and exact executable image actually loaded, even
  when its path or file is replaced concurrently;
- the exact host and per-invocation account, with one correlated credential
  generation for the complete operation;
- the effective configuration content and object identity actually opened,
  including proof that ambient configuration such as `http_unix_socket`
  cannot reroute traffic;
- the effective CA file or directory identity and contents actually consumed,
  with path, symlink, rename, replacement, and in-place mutation races covered;
- the exact current working directory object rather than a path that can be
  rebound before process start;
- stdin, bounded stdout/stderr, environment, locale, prompt suppression,
  timeout, cancellation, exit status, and redacted diagnostics;
- the process identity and no-descendant boundary through cleanup, including
  PID and process-group reuse resistance and proof that success, failure,
  timeout, and cancellation leave no surviving process.

Any input that cannot be bound through the object actually used by the
official CLI keeps the public result at `cli_unavailable`. A preflight hash
followed by path execution, a watcher that reports later mutation, or a
best-effort kill by recycled PID is not sufficient evidence.

## Prohibited designs

- Extracting, printing, passing, logging, or persisting a GitHub CLI token.
- Sending credentials or bodies to an unknown redirect authority.
- Accepting a query-bearing redirect.
- Starting a local HTTP listener, broker service, daemon, or callback server.
- Installing or requiring a privileged helper or system extension.
- Shipping a patched, forked, or repackaged replacement for the official
  installed GitHub CLI.
- Using a shell to launch the CLI or a credential helper.
- Using GitHub Actions for build, test, package, release, or verification.
- Enabling a partial mode while any eligibility gate or required evidence is
  missing.

### Original acceptance criteria (superseded)

- Every supported CLI selection names an exact official version/digest/source
  tuple, and launch-race tests prove those are the bytes actually loaded.
- Concurrent `gh auth switch`, active-user removal, unkeyed credential
  insertion, and page/redirect boundaries cannot change the selected account
  or credential generation.
- Unknown-authority and query-bearing redirect fixtures are rejected before
  any redirected credential or body transmission.
- Hostile ambient configuration, including `http_unix_socket`, cannot reroute
  a request, and the actual opened configuration and CA objects match the
  approved identities and contents throughout use.
- Rename, replacement, symlink, current-directory, executable-image, and
  in-place mutation races fail closed.
- macOS authentication succeeds under an enforced no-descendant boundary.
- Success, failure, cancellation, and timeout produce bounded, redacted
  results and leave no process alive; PID and process-group reuse cannot cause
  an unrelated process to be signaled.
- The parent process, arguments, environment, logs, diagnostics, UI, SQLite,
  and evidence never contain the CLI token.
- Until every criterion is directly satisfied on each enabled target, all CLI
  choices remain `cli_unavailable` and process-start instrumentation observes
  zero launches.
- All build and verification evidence is produced locally or on target-native
  release-operator environments, with no GitHub Actions involvement.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Exact reviewed image executes | Packaged broker and official CLI | Race executable path/file replacement and compare the loaded image with the approved version/digest/source/signing identity |
| Exact account stays bound | Multi-account fixture and official credential store | Race account switching and fallback slots across identity, pagination, retry, and redirect boundaries |
| Redirect authority is preflighted | Official CLI redirect surface | Prove unknown-authority and query-bearing redirects fail before redirected credential/body transmission |
| Configuration and trust inputs are bound | Packaged broker with hostile config/CA fixtures | Race socket, config, CA, symlink, rename, and in-place changes and inspect the objects actually consumed |
| Process containment is complete | Target-native process observer | Prove macOS auth needs no descendant and all completion paths resist PID/group reuse and leave no survivor |
| Token remains CLI-owned | Canary credential audit | Scan parent memory interfaces, arguments, environment, output, diagnostics, persistence, UI, and evidence without printing the canary |
| Disabled remains zero-launch | Public application composition | Remove any one eligibility guarantee and observe `cli_unavailable` plus zero process starts |

## Documentation impact

Before enablement, update the host/authentication architecture, security threat
model, build and test guide, supported CLI version/digest/source policy,
configuration and CA trust contract, account-selection UX, process-containment
design, diagnostics/redaction policy, verification ledger, and issue board.
Document unsupported platforms explicitly rather than weakening a gate.

## Adopted affected-installation qualification scope — 2026-10-07

The owner selected qualification of their Git and restoration of exact file viewing, registered in [RFC-BPR-0065](RFC-BPR-0065-ghes-exact-file-comparison.md). This authorizes extending the exact Apple Git constraint only after direct acquisition, confinement, bounds and exact-object evidence for the precisely identified installation. The current admitted installation remains unchanged until that evidence; no arbitrary version range or new platform is qualified by adoption. Actor-login snapshot rejection has its own attributable reproducer and evidence-backed repair. Existing CLI/account/credential/confinement/cancellation contracts and historical accepted cards/plan exits remain intact. BPR-1319–1323 owns the new runtime/native/owner evidence; both private diagnostic logs remain outside commits.

## Owner capability-based Git support amendment — 2026-10-07

The later [RFC0065 owner amendment](RFC-BPR-0065-ghes-exact-file-comparison.md#owner-support-policy-amendment--later-2026-10-07)
supersedes the exact Git-banner admission policy and the earlier
installation-only extension above. Implement the required Git operations with
bounded capability checks and a documented minimum only where necessary.
RFC0065 integrates a Git 2.49 minimum for `clone --revision` and bounded
required-capability admission, followed by the existing exact clone/object/pack
checks. The old exact banner remains historical qualification evidence; this
policy alone is not qualification of every newer installation.
Existing CLI opt-in, process confinement, bounded stores, exact objects,
credentials, account identity, cancellation and stale-result requirements
remain. New installed-tool and real affected-PR evidence belongs to BPR-1320
and BPR-1322; old accepted cards and immutable plans are unchanged.

## Owner per-account configuration amendment — 2026-10-09

The owner asked (chat, 2026-10-09) to "pin each gh call to a chosen account,
and let the Accounts tab recognise every gh-authenticated account", then chose
the design that keeps the token rule intact: "Go with B" ([[BPR-1373]]).

- **Inventory.** Account discovery may run `gh auth status --json hosts`
  without `--active`, under the same write-denied boundary, and admit every
  account per host as host, login, whether it is active, whether `gh` reports
  it signed in, and whether its token is in the keychain. `--show-token` is
  still never passed. It runs only on an explicit CLI gesture or after a
  pinned session is refused, never merely because a surface opened.
- **Per-call private configuration.** A CLI profile chosen for one account
  runs every `gh` invocation, including the bounded clone and the
  `gh auth git-credential` helper it launches, with `GH_CONFIG_DIR` naming a
  private directory created for that call alone. The app writes it before the
  launch: owner-only and read-only, holding `config.yml` (`version: "1"`) and a
  `hosts.yml` naming only that host and login. It removes the directory when
  the call ends. `gh` then resolves that account's keychain token itself. The
  directory holds no credential, and the global active account is neither read
  for the call nor changed.
- **The token rule stands.** BetterPullReview never extracts, prints, passes,
  logs or persists a GitHub CLI token, and never runs `gh auth switch` or
  `gh auth token`. Only the child's `GH_CONFIG_DIR` is added. No `GH_TOKEN`,
  `GITHUB_TOKEN`, `GH_ENTERPRISE_TOKEN` or `GITHUB_ENTERPRISE_TOKEN` is ever
  set.
- **Proof.** `gh` falls back silently to the host's unkeyed (active) token
  when the named login has no keychain entry, and its own status then reports
  that login as signed in. A pinned profile is bound to its host, login and
  the numeric account ID first observed for it. Every read is held to that
  account by [[BPR-1370]]'s viewer check. A sign-in or read answered by any
  other account is refused, the lease is refused for good, and the profile
  shows its account signed out in `gh`. It never falls back to the active
  account.
- **What changes.** A pinned call no longer reads the user's ambient
  `config.yml` or `hosts.yml`, so their `http_unix_socket` and other settings
  do not apply to it. An account whose token `gh` stores in its configuration
  file instead of the keychain cannot be pinned this way and is shown as such.
  A profile without a chosen account keeps the active-account behaviour above,
  with its checks. The redirect, CA, executable, keychain-helper and process
  risks recorded above are unchanged.

## Owner remembered-connection amendment — 2026-10-12

A successful explicit CLI selection and read-trust acceptance may be remembered per BPR profile/server under RFC-BPR-0071. That durable record permits bounded targeted validation of the saved host/login and numeric account on startup or exact-host navigation, without inventory or another read-trust question. It grants no accepted session, private-state access or publication authority. Every current tool, platform, per-call private configuration and viewer check remains required. Sign out disables automatic reuse; Forget removes the choice. Failed validation retains the choice and never falls back. Historical acceptance and the old CLI plan remain unchanged; the new restart scope has its own acceptance.
