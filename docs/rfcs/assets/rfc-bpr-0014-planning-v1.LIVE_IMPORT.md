# RFC-BPR-0014 planning import

This scoped rehydration adopts the already-imported pack-v2 RFC and immutable
issue-plan stages into live work. It is not a new requirement or acceptance amendment.

- Run: `rfc0014-20260920`; segment: `m1-planning`; mandate: `r1`.
- Engineering/bookkeeping/product-edit owner: `/root/rfc0014_m1`.
- Source checkout and tracker: `/Users/jaeger/Documents/GitHub/BetterPullReview`,
  branch `main`; the pre-existing 30 commits ahead of origin are retained.
- RFC_BASE_SHA: `d0a8a6dda005af12b3818234f4878a9f48a44146`, the verified clean
  start of this dedicated run. No prior dedicated RFC-0014 baseline/closeout
  was found in the live queue or scoped history.
- Original normative import: `61ac02ea3125ada107c0709626cd6b5d884ff452`;
  parent: `470741a605ac401464d9e50480667ca45033ca66`.
  This historical provenance is distinct from this run's comparison baseline.
- Source issue plan:
  [immutable original](../plans/RFC-BPR-0014-issue-plan.md),
  SHA-256 `970138245da8eb437a4226b2d2620ef71beef531cba938d307619676ad03632b`.
  [Retained import copy](rfc-bpr-0014-planning-v1/IMPLEMENTATION_ORDER.md) is byte-identical.
- [Normative RFC snapshot](rfc-bpr-0014-planning-v1/RFC-BPR-0014.source.txt),
  SHA-256 `d18a6cbc322ccc083bfdba91dd07f2ef3460590b0c7e77d8732afd3c9a6b4a32`,
  preserves the pre-adoption live RFC including its updater amendments.
- [Normalized proposal](rfc-bpr-0014-planning-v1/proposal.json),
  [preparation receipt](../plans/rfc-bpr-0014-release-readiness/preparation.json),
  [adoption receipt](../plans/rfc-bpr-0014-release-readiness/source.json) and
  [live plan](../plans/rfc-bpr-0014-release-readiness/milestone-plan.json)
  retain authored intent. The proposal uses allocated live IDs; adoption's
  mechanical ID map is therefore empty.

## Authored stage to live work

| Source stage | Live card |
|---|---|
| 1 Pack import and planning | BPR-0951 |
| 2 Architecture allocation | BPR-0952 |
| 3 Deterministic functional core | BPR-0953 |
| 4 Imperative application host | BPR-0954 |
| 5 Qt presentation adapter | BPR-0955 |
| 6 Composition and public-path integration | BPR-0956 |
| 7 Focused tests and fixtures | BPR-0957 |
| 8 UX/UI verification and clean native package matrix | BPR-0958 Apple Silicon; BPR-0959 Intel; BPR-0960 Windows; BPR-0961 Linux |
| 9 Security and boundary verification | BPR-0962 |
| 10 Agent and product documentation impact | BPR-0963 |
| 11 Simplification | BPR-0964 |
| 12 Verification and RFC closeout | BPR-0965 |

Allocation used `python3 scripts/allocate_issue_id.py --count 15 --reserve-for
rfc-bpr-0014-main`; the counter and immutable
`docs/issues/_reservations/BPR-0951--BPR-0965--98dad04c0678.json` receipt
were committed in `d3d707ac`. BPR-0951 was claimed in `cec196fb`.
The authored source is immutable, and MILESTONES.md is generated from the live JSON.

## Reconciliation and preserved obligations

The exact `rfcs` associations are the seven Done prerequisites BPR-0253,
BPR-0324–0328 and BPR-0908. They cover icons, macOS product identity and a
bounded private macOS release path; they do not close the RFC. Incidental text
mentions of RFC-0014 on other cards are not imported as RFC-owned obligations.
Their historical acceptance remains untouched.

The normative architecture already resides in the repository: the root guide,
product contract, system architecture and deterministic-core/imperative-host/Qt
allocation remain governing. This milestone imports links and accountable
verification obligations rather than replacing those documents.

The canonical index permits available-target implementation through RFC-0014.
Terminal closeout remains dependency-ordered; BPR-0965 depends on BPR-0617.
The RFC remains Proposed. The RFC-0035 qualified macOS updater exception and
owner amendment r3 remain in the live RFC. The updater's Intel deferral does not
waive RFC-0014's independent Intel package criterion. BPR-0911/0912 and
BPR-0950 retain their own scope.

All twelve source stages and six suggested implementation slices are covered.
M8/M9/M10 have independent post-M7 prerequisites so security/documentation
work can continue after safe release of a native or human-pending candidate.
The stage order remains the default. No required acceptance is excluded.
No product source, package artifacts, credentials, logs or review media are
imported; the other thirteen RFCs and unrelated worktrees remain outside this
milestone's editing scope.

## Routing observation

[Resolved selection](../plans/rfc-bpr-0014-release-readiness/strategy-selection.json):
coordinator-implementer recipe revision 2,
fingerprint `sha256:1ca88dfb502f12d72e9c547d366cd7c2e619504f6fb7f832eb9f2a691b8e27f7`.
Coordinator preflight reported `incomplete/state.drift`: MASK does not declare
the checkout workflow and reports `artifact.corrupt`; snapshot host loading is
`not_verified`. The running assigned native role reports
`rfc-implementer | gpt-6-astra | max`. Configuration completeness is not
proof of observed provider loading; this milestone does not repair shared tooling.

## Owner amendment r2

Before M1 handoff, the owner explicitly required inventory and reuse of the
existing project `publish-release` skill and real macOS release scripts. The
[source inventory](../../verification/RFC-BPR-0014.md#owner-amendment-r2-existing-macos-release-workflow)
records exact fingerprints, implemented mechanisms and evidence limits.
The checked future-only M2/M6/M9 plan amendment preserves the original source,
all selectors/exits, platform and closeout requirements, and publication authority.
The current handoff and renewed checkpoint use mandate r2; the r1 records above
remain original import history.
