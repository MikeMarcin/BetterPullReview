---
schema: "implement-rfc.card.v1"
id: "BPR-0946"
title: "Bind updater core policy and host contract"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: []
milestone: "RFC-0035 milestone 1"
metadata: {"milestone_plan":{"id":"rfc0035-auto-updater","path":"docs/rfcs/plans/rfc0035-auto-updater/milestone-plan.json"}}
acceptance: [{"criterion":"AC-01","outcome":"pass","revision":"42a85cf71bfe826a76c44dfe98e982a0bdc8c76b","evidence":{"kind":"observation","detail":"Fixed source/native release lease, API and redirect contract, stable grammar, exact universal ZIP identity and limits are bound in docs/architecture/release-updater.md and narrow source contracts; provider constants are confined to GitHub adapter.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-02","outcome":"pass","revision":"42a85cf71bfe826a76c44dfe98e982a0bdc8c76b","evidence":{"kind":"observation","detail":"core_release_update passes version/asset/digest/size selection and complete candidate/context stale rejection.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-03","outcome":"pass","revision":"42a85cf71bfe826a76c44dfe98e982a0bdc8c76b","evidence":{"kind":"observation","detail":"core_release_update passes automatic/manual/rate scheduling, failed attempt spacing, Later, Skip/newer versions, preference restart and persisted/monotonic clock boundaries; core_dependency_boundary passes.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-04","outcome":"pass","revision":"42a85cf71bfe826a76c44dfe98e982a0bdc8c76b","evidence":{"kind":"observation","detail":"application_release_update passes the public fake-port host course and absent-capability cut, typed failures, revoked/replaced authority, reorder, cancel/disable/all-phase shutdown, bounded jobs/ingress, progress coalescing, projection and executor failure; install remains disabled.","reference":"docs/verification/RFC-BPR-0035.md"}},{"criterion":"AC-05","outcome":"pass","revision":"42a85cf71bfe826a76c44dfe98e982a0bdc8c76b","evidence":{"kind":"observation","detail":"Core-only configure/full build and focused integration pass 4/4; both architecture contract suites and clean candidate export architecture/core scans pass; eight-file formatting, four-TU tidy plus final test tidy, private header syntax, documentation gate and 64 documentation tests pass.","reference":"docs/verification/RFC-BPR-0035.md"}}]
---

# Bind updater core policy and host contract

## Scope

First authored milestone of [[BPR-0911]]. Implement deterministic release,
version, exact universal ZIP selection, scheduling and command policy, plus a
narrow application host exercised with fake ports. No production transport,
credential retrieval, UI, package replacement or release publication.

## Acceptance

- AC-01: Bind the fixed private distribution source, native GitHub.com release
  access, API/redirect contract, version grammar, exact asset identity and limits.
- AC-02: Core tables reject malformed/equal/older/prerelease/draft releases and
  ambiguous/incompatible assets; candidate identities include release, tag,
  asset, size and optional digest.
- AC-03: Deterministic time facts implement daily and manual admission, failed
  attempt spacing, rate limits, Later, exact-version Skip, disabled/managed and
  unsupported capabilities. No policy reads clocks or performs effects.
- AC-04: The public host with fake narrow ports exercises check, offer, download,
  verification and failure projections; every async completion is bound to its
  operation/generation/access/candidate. Account replacement, disable, cancel and
  shutdown reject stale and duplicate completion. Install stays unavailable
  until a qualified installer is composed in milestone 3.
- AC-05: Core-only build, focused policy/host checks, architecture boundaries and
  documentation gate pass; no product-shipping or native-upgrade claim is made.

## Verification and execution record

Accepted milestone-1 source candidate:
`42a85cf71bfe826a76c44dfe98e982a0bdc8c76b` (macOS 26.6.2 arm64).
Implementation `64bbf9c1`, provider-boundary correction `ab5209b9`, final focused
coverage `42a85cf7`. Planning/claim commits are `ae388676` and `bb1e9bf4`.

- AC-01: [Component contract](../../architecture/release-updater.md) binds fixed
  source, native release lease, version/asset identity and bounds. Wire constants
  live only in the GitHub adapter. No production transport is composed.
- AC-02/AC-03: `core_release_update` passes grammar/overflow, numeric precedence,
  exact and duplicate assets, digest/size bounds, daily/manual/rate scheduling,
  Later/Skip/newer versions, preference restart and clock normalization tables.
- AC-04: `application_release_update` passes the real host with fake narrow ports:
  accepted check/download/verification projection, absent-capability negative
  control, typed failures, complete candidate/lease binding, reordering,
  revocation, all active-phase shutdowns, cancellation, bounded jobs/ingress,
  coalesced progress, projection exceptions and executor refusal.
- AC-05: `cmake --preset core-only` and `cmake --build --preset core-only` pass.
  Focused core/host/foundation/core-boundary integration is 4/4. Both boundary
  contract suites pass. Architecture and core scans pass on the exact candidate
  exported from Git, without the unrelated nested worktree. Formatting (8 files),
  clang-tidy (4 TUs, then changed test TU), documentation gate and documentation
  tests (64) pass. The private wire header also passes standalone C++23 syntax.

The original checkout architecture scan is contaminated by the existing nested
`.claude/worktrees/agent-a17cae88115dbbb4e`; it remains untouched. The first clean
export caught and led to correction of misplaced endpoint constants. See the
[verification ledger](../../verification/RFC-BPR-0035.md) for exact limits.
No reviewer, attended session, background command or native install is pending
within this component milestone. RFC AC-01/03/04/08 receive component evidence
only; full UI, persistence, native service/staging/installation acceptance remains
on [[BPR-0912]]. Next: [[BPR-0947]].
