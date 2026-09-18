---
schema: "implement-rfc.card.v1"
id: "BPR-0912"
title: "Verify and close RFC-BPR-0035"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0911"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0035"]
---

# Verify and close RFC-BPR-0035

## Scope and continuation

[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md) is registered only.
Next: await an implementation request, then split [[BPR-0911]] by the authored
milestones. No runtime evidence exists and no updater is enabled.

RFC base: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.

Planned cards: BPR-0911, BPR-0912; append later allocated implementation slices.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Manual and daily automatic checks, Later, Skip This Version and persisted preferences work through the shipped UI; managed and disabled installations make no automatic requests. | Composed menu/Settings, manual-clock scheduling, restart and managed-policy fixtures | Not run; proposal only | None |
| AC-02: Only a strictly newer stable release with one exact compatible asset is offered; malformed versions, drafts, prereleases, duplicate assets and stale completions are refused. | Core version/selection tables and out-of-order host completions | Not run; proposal only | None |
| AC-03: Private release access uses an explicitly selected GitHub.com authority; revocation cancels work, credentials never cross redirect origins, and inaccessible releases are not reported as up to date. | Private-release fixture, sign-out/account replacement, redirect and secret-canary tests | Not run; proposal only | None |
| AC-04: Downloads and staging enforce declared bounds and reject wrong identity, version, architecture, signer, digest, unsafe archive entries and changed staged candidates before replacement. | Adapter failure fixtures plus native signed-bundle validation on disposable artifacts | Not run; proposal only | None |
| AC-05: Installation quiesces every window and process using the target, preserves drafts and pending publication reconciliation, and refuses timeout without forced termination. | Composed multi-window shutdown, flush failure, active publication, competing-process and timeout course | Not run; proposal only | None |
| AC-06: Interrupted replacement and failed relaunch retain a recoverable old application; success requires a matching new-process receipt, and cleanup touches only transaction-owned resources. | Native disposable-install fault injection at each journal boundary, restart, rollback and cleanup course | Not run; proposal only | None |
| AC-07: The packaged macOS update path upgrades an older signed version to the offered signed version, relaunches with retained user state, and reports truthful progress and terminal status. | Target-native packaged upgrade on Apple Silicon and Intel, with exact artifact digests and observed UI | Not run; proposal only | None |
| AC-08: Core, host, Qt and platform boundaries hold; unsupported platforms and managed packages expose truthful manual fallback; release and security documentation describe the shipped authority and network paths. | Architecture/composition cuts, unsupported capability fixtures, package inspection and documentation gates | Not run; proposal only | None |

## Completion rules

Reconcile every criterion with actual commands, artifact digests, producing
commits and target-native results. Require linked implementation cards done and
RFC dependencies eligible in canonical index order before status changes. Run
`python3 scripts/verify_documentation.py --root .`,
`python3 tests/scripts/verify_documentation_tests.py` and focused public-path
checks. No full-product build is evidence for unexercised install/recovery paths.
Retain unsupported platform and unavailable native evidence explicitly.
