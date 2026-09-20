---
schema: "implement-rfc.card.v1"
id: "BPR-0912"
title: "Verify and close RFC-BPR-0035"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0911"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0035"]
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0035

## Scope and continuation

[RFC-BPR-0035](../../rfcs/RFC-BPR-0035-github-release-auto-updater.md) implementation
was requested on 2026-09-20. The current assignment is milestone 1 only;
[[BPR-0946]] owns the core and fake-host foundation. No updater is enabled.

RFC base: `1b4c24154bd1d4f1d4bea3d84c593e2fb4c3d47f`.

Planned cards: BPR-0911, BPR-0912, BPR-0946, BPR-0947, BPR-0948, BPR-0949.

## Current engineering checkpoint

Run `rfc0035-20260920`, segment `m1-implementation`, mandate revision `r1`
(implement RFC-BPR-0035; no amendments). Child `/root/rfc0035_m1` is the sole
milestone-1 repository writer on `main`. Initial HEAD:
`d06b6fb0272ce53bf452d87322c957f11f0f6cb1`; worktree was clean.
Selected strategy is `coordinator-implementer` revision 2; resolved snapshot:
`build/rfc0035-run/execution-strategy.json`. The installed workflow preflight
failed because it invoked obsolete `mask ensure`; the host exposes the assigned
native `rfc-implementer` role. This is a tooling observation, not acceptance.

Allocation reserved BPR-0946 through BPR-0949 for the four authored milestones.
Plan commit `ae388676`; BPR-0946 claim `bb1e9bf4`. The adopted plan is
`docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json`; exact binding is
`build/rfc0035-run/m1-checkpoint.json`. Current source candidate is uncommitted
on that claim: `release_update` core, application host/ports and focused core/host
tests with their CMake registrations. The early public fake-port course passed
`ctest --preset core-only -R '^(core_release_update|application_release_update)$'
(2/2), including absent-release-capability negative control. Initial compile
warnings (test range copy and lambda capture shadow) were corrected.

Next: extend failure, reordering, cancellation and shutdown checks; self-review;
run the core-only/architecture/documentation integration pass and finish BPR-0946.
No background commands, live mutations or protected updater candidates exist.
Milestones 2–4 and all full RFC acceptance remain pending.

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
