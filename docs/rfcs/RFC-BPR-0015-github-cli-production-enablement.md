---
id: RFC-BPR-0015
title: GitHub CLI Production Enablement
status: Deferred
area: BPR
milestone: Post-Pack v2
depends_on: [RFC-BPR-0014]
pack_version: post-v2
---

# RFC-BPR-0015: GitHub CLI Production Enablement

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

## Acceptance criteria

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
