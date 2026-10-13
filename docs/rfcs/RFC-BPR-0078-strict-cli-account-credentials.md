---
id: "RFC-BPR-0078"
title: "Use the chosen GitHub CLI account without credential fallback"
status: "Auto-Verified"
area: "BPR"
depends_on: []
metadata: {"mandate_revision":"strict-cli-publication-r2","rfc_base_sha":"aa3d71fe470627e1e78488eb51a9114a788f33bd","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"},"engineering_acceptance":{"status":"accepted","milestone":"strict-credential-qualification","source_revision":"2793606c5a7785d0b671b81a31f016b401cab1b5","evidence":"docs/issues/_checkpoints/strict-cli-account-publication-v1/m2-evidence.json","independent_receipt":"/tmp/bpr-strict-cli-run/critical-m2-review.md","terminal_status_owner":"confirmed on main after exact accepted integration","integrated_revision":"fd01bc16afb37798162587e254bcbdb9f1fef596"}}
---

# Use the chosen GitHub CLI account without credential fallback

## Adoption — 2026-10-13

The current owner mandate `strict-cli-publication-r2` authorizes implementation of the strict credential RFC followed by the publication companion, with independent critical plan and implementation reviews. It supersedes the earlier authoring-only statements retained below. The [live import](assets/strict-cli-account-publication-v1.LIVE_IMPORT.md) preserves originals, source identities, the reviewed proposal and findings. The new scope permits only the narrow transient credential exception described here; historical tokenless/per-write-warning criteria and BPR-1373 owner acceptance remain bound to their original candidates. The companion begins only after strict supported-macOS capability and evidence are accepted. No status is inferred from drafting.

## Problem and grounded current behavior

The owner wants a missing chosen-account credential to stop the operation,
rather than permit a request as another account and detect that afterwards.
In this conversation they accepted a token in memory if BPR does not persist
it, suggested investigating Keychain, rejected maintaining a gh fork, and
then explicitly limited the work to investigation and RFC authoring.

[BPR-1373](../issues/40-parked/BPR-1373-pin-each-gh-call-to-a-chosen-account.md)
implemented a private, token-free `GH_CONFIG_DIR` for each pinned launch.
[The launch path](../../src/adapters/github/src/gh_cli_transport.cpp)
names the host and login, while
[the provider](../../src/adapters/github/src/github_provider.cpp)
checks observed login and numeric account identity. Ordinary active-account
switching does not change a pinned launch's chosen username. However,
`gh`'s `ActiveToken` falls back to the host's unnamed Keychain entry when the
named lookup fails. BPR-1373's recorded live probe observed that fallback;
its fixtures exercise refusal after the wrong viewer is returned.

Source inspection of official gh 2.101.0 establishes a different path:

- [`auth token --hostname HOST --user LOGIN`](https://github.com/cli/cli/blob/v2.101.0/pkg/cmd/auth/token/token.go#L52-L90)
  uses `TokenForUser`, and reports an error when no token is found.
- [`TokenForUser`](https://github.com/cli/cli/blob/v2.101.0/internal/config/config.go#L524-L533)
  reads the named Keychain entry, then that user's configuration token; it
  does not consult the unnamed active-account entry. In BPR's private
  configuration there is no token, so the second lookup cannot supply one.
- [Token environment variables](https://cli.github.com/manual/gh_help_environment)
  override stored credentials. The
  [Git credential helper](https://github.com/cli/cli/blob/v2.101.0/pkg/cmd/auth/gitcredential/helper.go#L106-L132)
  uses `ActiveToken` too. Thus explicitly supplying the acquired token is a
  supported way to avoid later account selection for both API and helper use.

These are source findings, not new native execution evidence. No real token
was retrieved for this investigation. The
[proposed amendment](assets/strict-cli-account-publication-v1/proposed-cli-amendments.md) records the conflicts with
the adopted tokenless contract and the affected historical evidence.

## Proposed behavior and boundaries

### Acquire a specific credential and fail before a request

Every credentialed CLI operation must name a canonical host and an explicitly
chosen login. A remembered connection also supplies its previously accepted
numeric account identity, as defined by
[RFC-BPR-0071](RFC-BPR-0071-remember-cli-connections.md).
An old unpinned connection must obtain an explicit account choice before
credentialed use; it cannot retain an ambient-account escape path.

Run stock gh's closed `auth token --hostname HOST --user LOGIN` command under
the existing write-denied boundary, with the private host/login configuration
and the constructed token-free child environment. Capture its output through
a dedicated sensitive-result path, rather than a general retained command
output. The command obtains only the named account's existing Keychain
credential. Do not read a plaintext token from the owner's global gh files,
use the hidden `--secure-storage` flag as an undocumented dependency, or run
`auth switch`, `login`, `refresh` or `logout`.

A missing, inaccessible, empty, malformed or oversized credential, failed
retrieval, cancellation or shutdown refuses the operation before its API or
network Git launch. Token bytes must fit the existing 8 KiB per-environment-
value ceiling; capture is bounded accordingly. A failure never retries with
the active account, native credentials or an uncredentialed authenticated
operation. Local version/help probes and explicit account inventory remain
their existing separate operations and receive no acquired token.

### Bind the operation to the acquired bytes

Keep one immutable transient credential for an operation's identity validation
and credentialed launches. Validate the selected login, and an existing
session's numeric account, using those same bytes before publishing or
accepting account-private content. A credential filed under the wrong login
must not get a write. This preserves identity validation as a defence against
misbound credentials; it removes reliance on a check after the write to
detect active-account fallback.

Supply only the acquired token through the child-local `GH_TOKEN` for
GitHub.com or `GH_ENTERPRISE_TOKEN` for an admitted GHES host. Never copy an
ambient token variable. The same credential must reach any gh-owned HTTPS
clone, Git child and gh credential helper that serves that operation. Local
credential-free Git serving already acquired objects receives no token.
Retain the existing exact-repository helper/configuration restrictions.
Unavailable required command or helper behavior refuses the CLI operation;
it does not weaken selection or silently change authentication transport.

An active-account switch cannot change the credential of an admitted
operation. Removing its Keychain entry after acquisition cannot replace the
bytes already held in memory: the in-flight operation may finish with them,
or GitHub may reject a revoked token. Subsequent operations acquire afresh
and fail if the credential is absent. This is strict credential selection,
not atomic observation of later Keychain deletion or token revocation.

### Keep the token transient

Credential ownership stays inside the GitHub adapter. No token crosses to
core, Qt, application projections, profile metadata or publication grants.
There is no cross-operation credential cache. Release the credential and
clear owned sensitive buffers on success, refusal, cancellation, exception
and shutdown; account for descendants before their credential lifetime ends.

BPR must never write the acquired token to SQLite, private configuration,
temporary files, clone URLs/configuration, Git credential storage, recovery
journals, Keychain or another vault. It must never print it, include it in
arguments or retain it in diagnostics, tool output or test evidence. Adapt
the process runner's sensitive buffers and test seams so generic request,
environment and output recording cannot accidentally retain it. Document
unavoidable Qt/OS/child-memory copies and process-environment exposure
truthfully; this proposal does not promise prevention of swap or crash-dump
capture, or perfect erasure in stock gh and its children.

The owner's non-persistence requirement means BPR creates no durable token
copy. The existing credential may remain in the Keychain managed by gh.
Connection metadata remains token-free and retains its current restart,
Sign out and Forget semantics.

### Keychain investigation and scope

Use gh's existing account-specific Keychain retrieval for this proposal.
[gh's macOS documentation](https://github.com/cli/cli/blob/v2.101.0/docs/macos-keyring.md)
describes its `/usr/bin/security` lookup and Keychain access controls. A
direct native read by BPR would introduce a different accessing application
and require native access-control qualification. The existing
[BPR native vault](../../src/adapters/github/src/macos_keychain.cpp)
uses its own service namespace; it is not an existing reader of gh's items.
Creating a BPR-owned Keychain token copy would persist it and is outside the
selected non-persistence design. Direct read-only access to gh's exact
host/login item is a possible later alternative, not a prerequisite.

Preserve process confinement, bounded responses, exact host/PR/head/draft
admission, cancellation, server reconciliation, rate budgets, and current
platform eligibility. Reuse existing identity observations where compatible
with the same-credential lifetime; account for any additional preflight
request in the operation's rate and time budgets. Use supported stock gh;
qualify the required command shapes locally rather than assuming that the
existing 2.96 floor proves their behavior. No fork, new stored credential,
native-transport migration or platform expansion is proposed.

## Acceptance criteria

- **AC01:** With two accounts available, switching the global active account
  before retrieval, between validation and publication, and during helper
  use leaves reads and writes attributed to the chosen account. Removing
  its credential before retrieval causes zero API/network Git launches,
  despite a usable unnamed credential. A wrongly labelled credential
  causes zero writes.
- **AC02:** The composed review, inline-comment and tour publication paths,
  paginated reads, exact-blob reads and clone/helper path all use strict
  acquisition. Unsupported required behavior, missing account choice and
  credential failures expose typed refusal without another authority.
- **AC03:** A synthetic canary is present only in permitted transient
  credential buffers and credentialed descendant environments. It is absent
  from arguments, retained outputs/logs, settings, configuration, object
  stores, recovery state and all BPR-created persistent stores. Failure,
  cancellation and shutdown observations cover sensitive-buffer and child
  lifetime handling; third-party memory limits remain explicit.
- **AC04:** Direct official-gh evidence on supported macOS confirms named
  lookup failure, selected-account API use, helper inheritance and unchanged
  global account state. Use disposable synthetic credentials for local
  negatives and designated BPR-Test for any live read/publication proof;
  evidence records outcomes and rate headers without recording tokens.
- **AC05:** Existing focused identity, admission, recovery, cancellation and
  architecture checks pass with narrowly revised credential-boundary rules.
  Documentation states the transient exception and supported-tool/native
  evidence accurately. Historical tokenless evidence is preserved with its
  original scope.

## Engineering acceptance — 2026-10-13

All AC01–AC05 and `strict-credential-qualification` exits are accepted on frozen
candidate `2793606c5a7785d0b671b81a31f016b401cab1b5` under `strict-cli-publication-r2`.
[The qualification ledger](../verification/RFC-BPR-0078.md) and
[revision-bound evidence](../issues/_checkpoints/strict-cli-account-publication-v1/m2-evidence.json)
retain direct native, strict fixture, persistence/lifetime, focused gate and
independent critical receipts. The final bounded review reports no blocker and
confirms production-null hooks/shared seam ownership and the recorded hashes.
Its native acceptance preserves explicit local metadata-socket/Git-CA/CONNECT
instrumentation, live `/user` scope and watchdog-versus-kernel-failed-kill limits.
No live-publication/full-Qt-native, escaped-child or perfect-erasure claim is made.

Main integration owns terminal RFC/index confirmation. The lane leaves their
existing Proposed/Proposed pair consistent while recording actual engineering
acceptance. BPR-1427 is finished with supplied acceptance; the companion can now
start through a fresh milestone owner. Historical BPR-1373 owner judgment and
imported tokenless plans/evidence remain unchanged.

## Main integration confirmation — 2026-10-13

The accepted strict source `2793606c5a7785d0b671b81a31f016b401cab1b5`,
tracker `4ae67d6b59db79e83bc59eb0550cf7c5f8c9efb0`, and final independent
critical receipt were integrated on main at `fd01bc16afb37798162587e254bcbdb9f1fef596`.
The monitor's exact candidate passed ten focused checks, both direct strict
review/tour selectors, the documentation gate and all 67 documentation contracts.
All 42 protected controls/draft hashes were preserved. This terminal status
confirms RFC AC01–AC05 acceptance recorded in the strict verification ledger and
M2 handoff, with their stated native and memory limits. It supplies no companion,
release or historical BPR-1373 owner verdict. Receipt:
`/tmp/bpr-strict-main-landing.json`.
