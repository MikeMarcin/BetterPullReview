---
id: BPR-0025
title: Implement hardened GitHub CLI broker transport
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0019, BPR-0020]
completed: 2026-07-30
---

# Implement hardened GitHub CLI broker transport

Invoke a validated absolute `gh` executable through an injectable process
port, use an argument vector and standard input, disable prompts and ambient
repository selection, bound time and output, classify failures, and redact all
retained diagnostics.

The broker must never extract a token, place credentials in arguments, use a
shell, or expose credential-bearing environment state to unrelated code. It
must run from a canonical directory that is currently outside any worktree or
bare-repository ancestry, with an explicit minimal environment allowlist,
explicit hostname/endpoint/method, and an argument allowlist that rejects
token/status-output, response-cache, debug, and credential-bearing flags.
Inject unknown canary variables to prove they do not reach the child.
Canonical/currently non-repository validation alone does not establish
owner-controlled permissions or bind directory identity through launch;
BPR-0037 owns that production-enablement requirement.

Directly prove the selected CLI version rejects certificate failures and
unknown-authority redirects without forwarding a request body or credential.
If those semantics cannot be proved, CLI mode remains a closeout blocker rather
than inheriting the native HTTPS adapter's evidence.

GitHub Actions is not an acceptable build or evidence surface.

## Result

- Added a standard-C++ correlated wire transport and injectable process port.
  Every request and terminal retains the complete applicable session or
  pull-request parent, profile revision, host, operation, and page.
- Added a closed compiler for validated GitHub.com and GHES 3.18 `gh api`
  reads. It emits only translated relative endpoints, explicit
  hostname/method, canonical noncredential headers, and standard-input bodies.
- Validated one canonical absolute `gh` and one canonical directory currently
  outside any worktree or bare-repository ancestry, plus effective
  executability, typed platform credential-session facts, selected proxy/trust
  facts, and complete per-value and aggregate argument/environment bounds.
  Directory ownership, permissions, identity binding, and validation-to-launch
  replacement remain pre-enable work in BPR-0037.
- Constructed each child environment from nothing. Unknown parent variables,
  tokens, authentication overrides, repository/host overrides, debug, status,
  cache, pagination, templating, shell, prompt, update, spinner, editor,
  browser, pager, TTY, and credential-bearing proxy surfaces are absent.
- Added the sole production `QProcess` runner behind one runner-owned
  serialized worker. Absolute deadlines include queueing and are rechecked
  immediately before launch. A compiled seam test proves completion-watchdog
  quarantine, rejection of another request, zero launch, and recovery. Source
  and hostile architecture gates separately require an actual failed bounded
  reap to retain the live process on the quarantined worker for continued
  reaping; the compiled runtime suite does not force that live-process path.
- Enforced allocation-free request-shape bounds and pre-cancellation before
  the worker allocates or copies a request. Path, argument, environment,
  standard-input, stream, time, and reap N+1 cases make zero child launches.
- On Unix, launch requests closed inherited descriptors, disabled core dumps,
  reset signal handlers, and a new session whose child is the initial
  process-group leader. Compiled child observation proves all four facts.
  Compiled lifecycle cases prove that clean exit, failure, timeout, and
  cancellation signal descendants that remain in that initial process group
  and reap the direct child; an escape case proves a descendant that moves
  into another process group survives this cleanup before the fixture removes
  it. Source and hostile architecture gates additionally enforce process-group
  resolution, shutdown, and actual failed-reap retention structure.
- Bounded public reads use a fixed buffer and request at most the remaining
  accepted limit plus one byte. Only clean success exposes retained stdout;
  every failure discards partial output, stderr, paths, exit details, and raw
  Qt diagnostics.
- Directly characterized an owner-private, digest-stable copy of selected
  `gh` 2.95.0. It rejected the self-signed certificate before HTTP but followed
  a cross-authority `302`; the selected origin received the exact synthetic
  authorization/body and the distinct sink received neither. `307`/`308`
  sink absence remains explicitly unproven.
- Identified three additional source-backed blockers: production path/name
  validation cannot bind the launched image to the probe's exact digest;
  persisted `http_unix_socket` can remain ambient through CLI account/config
  paths and canonical Linux CA paths do not bind trust-material identity or
  contents through the later open; and `gh api --hostname` cannot bind the
  child to the correlated account generation while `gh auth switch` can
  change the active account.
- Recorded three additional pre-enable process risks in BPR-0037: a descendant
  can leave the initial Unix process group, and canonical/currently
  non-repository directory validation does not establish owner-controlled
  permissions or bind directory identity across the launch boundary. Saved
  numeric PID/process-group identity is also not bound against reuse between
  leader exit and later group cleanup, so group signaling is not claimed
  race-free. BPR-0037 also owns hostile CA-file/CA-directory replacement and
  content-mutation closure before enablement.
- Production therefore exposes all four typed blockers, returns
  `cli_unavailable`, and starts zero processes even with a fake successful
  process port. BPR-0037 owns production CLI enablement.
- Scoped the Windows claim honestly. Explicit empty `PATH` and `SystemRoot`
  validation are prepared, but Job Object descendant containment, Qt private
  pipe bounds, crash artifacts, and target-native execution remain BPR-0036
  work. They do not block the completed macOS/Linux implementation slice.
- Kept all build and verification ownership local or target-native. GitHub
  Actions has no role.

## Evidence

- macOS focused transport/runner matrix: 2/2 passed; runner lifecycle
  repetition: 10/10 passed. The compiled Unix cases cover descendants that
  remain in or escape the initial process group, Unix hardening facts, and
  watchdog quarantine; they do not force actual failed bounded reap with a
  live `QProcess`.
- Linux focused transport/runner matrix: 2/2 passed under GCC in the existing
  Ubuntu 26.04 x86_64 full-system guest; runner lifecycle repetition: 10/10
  passed with the same scope: hardening, process-group, and watchdog behavior
  are runtime-covered; actual live-process failed reap is not forced.
- The exact selected-binary trust probe passed its fixed structural checks and
  reported `broker-availability: unavailable` with the expected digest,
  certificate alert, `302` follow, and bounded `307`/`308` observations.
- Hostile architecture-boundary suite: 87/87 passed; the direct CMake verifier
  and architecture CTests passed. This source-gated evidence owns the actual
  failed-reap live-process retention and continued-reap claim.
- Complete macOS developer matrix: 30/30 passed; core-only matrix: 16/16
  passed. Documentation and original source-pack validators, `git diff
  --check`, and tracked/on-disk GitHub Actions workflow-absence gates passed.
- Frozen Linux source passed the hostile architecture suite 87/87 and direct
  CMake boundary verifier in addition to the focused/repeated compiled tests.
- Final adversarial security and simplification reviews reported no remaining
  P1/P2 blocker. The simplification pass directly drove the predicate
  completion wait, watchdog quarantine, pre-copy helper hardening, and removal
  of a redundant correlation guard.
- No `better_pull_review_github_cli_process_runner_tests` crash report remains
  after the self-`SIGKILL` fixture runs.
- Windows target-native process evidence and CLI production trust remain
  explicit open tasks in BPR-0036 and BPR-0037 respectively.

## Post-completion scope resolution — 2026-07-30

BPR-0037 subsequently resolved, rather than enabled, the production CLI path.
By product-owner approval, native HTTPS is the only milestone-one production
authentication transport. All visible CLI choices remain fixed at typed
`cli_unavailable` before CLI executable/configuration discovery or process
creation, and the positive enablement contract moved outside RFC-BPR-0002 to
deferred RFC-BPR-0015.

The current official `gh` 2.96.0 audit strengthened the original result:
`gh api` has no per-invocation account selector, active-account/token
alternatives violate the identity or token-ownership contract, ambient
`http_unix_socket` directly reroutes an authenticated API request, a
cross-authority `302` is followed, and macOS keyring access starts
`/usr/bin/security`. The dormant broker and runner remain bounded test
surfaces, but no positive production or Windows CLI execution claim is part
of RFC-BPR-0002. BPR-0036 now retains only the independent Windows native
HTTPS, Credential Manager, public-composition, runtime, and dependency
evidence.
