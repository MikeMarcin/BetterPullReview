---
schema: "implement-rfc.card.v1"
id: "BPR-0598"
title: "Inventory personal discovery and URL flow and plan RFC-BPR-0026"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0597","BPR-0244"]
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
acceptance: [{"criterion":"Re-audit the live RFC dependencies, accepted scope, governing architecture and current code/verification paths; record concrete ownership and migration facts","outcome":"pass","revision":"2722bd67","evidence":{"kind":"observation","detail":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md records, at planning head 2722bd67: every RFC-0026 dependency Auto-Verified (RFC-BPR-0010/BPR-0244 under r4); RFC-0028, the owner's 2026-09-11 Home direction (BPR-0732/0733) and RFC-0032 superseding the Home presentation, Resume action and automatic restoration; provider search limited to the snapshot's is:open search (github_read_contract.cpp:398-401); queue refresh replaying opened snapshots with role none (github_application.cpp:757-786); unknown activity projecting quiet (review_queue_enrollment.cpp:203-206); no follow, no whole-queue refresh (BPR-0801); parse_setup_destination text locations all reported unavailable (setup_destination.cpp:41-127, :260); setup open reloading the open pull request; existing setup, public preview, CLI link, recents, saved repositories and per-window view state."}},{"criterion":"Map every numbered RFC acceptance criterion to a bounded implementation slice and a direct planned check, naming the smallest existing selectors or the focused new check, including one composed public path for UI behavior","outcome":"pass","revision":"2722bd67","evidence":{"kind":"observation","detail":"Criterion map in the reconciliation record and RFC-BPR-0026's planning table: AC-01 and AC-09 no slice with named re-run selectors (application_profile_window_review_restoration_public_path, application_profile_multi_window_public_path, qt_composed_github_scale_*, qt_home_list_scale_*, core_setup_destination, application_setup_destination, application_setup_destination_public_path); AC-07/08 BPR-0987; AC-03/05/06 BPR-0988; AC-02..06 BPR-0989 (core, manual-executor host, sqlite_queue_enrollment_storage); AC-02/04/05/10 BPR-0990 with the composed public-path course over at least 2,000 unrelated open pull requests and two windows. Superseded parts (automatic restoration, tabs, Home presentation) are recorded as superseded, not passed."}},{"criterion":"Allocate only needed slice cards, link their dependencies and affected RFC, and update the closeout's planned cards/checklist without claiming tests ran","outcome":"pass","revision":"2722bd67","evidence":{"kind":"command","detail":"python3 scripts/allocate_issue_id.py --count 4 --reserve-for rfc0026-planning-reconciliation reserved BPR-0987..BPR-0990 (commit 2722bd67). BPR-0599 depends_on BPR-0598, BPR-0987..0990; planned cards and planned checks updated, results left not run. Plan ux-redesign-pack-0028 close-rfc-0026 amended via partial registration then check-amendment (sha256:16c96d3a... -> sha256:6637b906..., affected close-rfc-0026 only; published before sha256:820fe9d9...). verify_documentation.py DOCUMENTATION_OK, git diff --check clean, issue_board validate --rfc RFC-BPR-0026 valid over 982 cards."}}]
---

# Inventory personal discovery and URL flow and plan RFC-BPR-0026

## Objective

Trace repository search in PR loading, current queue observations/membership, viewer identity, provider capability contracts, pending setup, and workspace opening. Define bounded personal discovery and exact URL-to-view routing without broad PR hydration. Allocate the first coherent slice and its fixture/provider/public-path selectors.

Governing design: [RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md).
Closeout: [BPR-0599](BPR-0599-close-rfc-0026.md).

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 4 of 4.
Planned cards: BPR-0598, BPR-0599. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Acceptance

- Re-audit the live RFC dependencies, accepted scope, governing architecture, and
  current code/verification paths; record concrete ownership and migration facts.
- Map every numbered RFC acceptance criterion to a bounded implementation slice
  and a direct planned check, naming the smallest existing selectors or the
  focused new check required. Include one composed public path for UI behavior.
- Allocate only needed slice cards, link their dependencies and affected RFC,
  and update the closeout's planned cards/checklist without claiming tests ran.

## Non-goals

Product implementation, RFC closeout, live authentication or publication, broad
validation matrices, and redesigning the approved interaction model are not part
of this entry card. Later implementation authority does not waive the RFC's
existing dependency/eligibility rules.

## Verification record

Not run; entry work and product implementation are deferred. When authorized,
run the documentation gate on the planning slice, record exact focused selectors
for its implementation consumers, and commit with explicit paths. This card is
not satisfied merely by the initial RFC registration.

## Scoped reconnaissance, 2026-09-08

The original broad repository search still exists in
`src/adapters/github/src/github_paged_read.cpp`: the full PR read constructs
`search_pull_requests` with `SearchTerms = "is:open"` and reads SearchResults
before returning its snapshot. A saved-repository personal discovery scheduler
must be independent of this full-review path; filtering that snapshot is not
AC03/05 evidence. Existing core queue roles distinguish author/requested reviewer
but do not by themselves prove discovery before opening a PR.

BPR-0689 supplies the exact unknown-server/signed-out pending destination bridge
under RFC0018. It preserves unsupported sublocation context with an explanation;
exact accepted file/comment/commit mapping, profile/window Resume and personal
discovery remain RFC0026 work. Query parameters must not route credentials or
provider requests. This note is intake evidence, not a completed inventory or
product acceptance. No source or product check ran for this reconnaissance.

## Inventory and plan, 2026-09-22

Run `main-closeout-20260922`, milestone `plan-rfc-0026`, sole non-delegating
`rfc-implementer` on `main` (coordinator-implementer), bound at `70f6b16b`
(`build/bpr0598-bind.json`). The complete record, with code references at
planning head `2722bd67`, the per-criterion map and the plan amendment, is the
[RFC-0026 planning reconciliation](../../rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md).

- **Re-audit.** Every RFC-0026 dependency is Auto-Verified, including the
  plan's named canonical predecessor RFC-BPR-0010 ([[BPR-0244]], owner r4).
  RFC-BPR-0028 (owner lock 2026-09-06), the owner's 2026-09-11 Home direction
  ([[BPR-0732]], [[BPR-0733]]) and RFC-BPR-0032 (owner accepted 2026-09-16)
  supersede this RFC's Home presentation, its Resume action and its
  automatic restoration of other windows where they conflict; browser-style
  tabs stay unscheduled. Shipped and reused: the Home button, the grouped
  list and its per-window view state, Recently viewed with Resume, the
  one-field Open bar and its parser, the exact pending setup destination,
  signed-out public reading, CLI-session links and shared sign-in, saved
  repositories and servers.
- **Real gaps.** No read discovers authored or requested pull requests (the
  only search is the snapshot's repository-wide `is:open`); the queue learns
  only from opened snapshots with no role; unknown detail projects as quiet;
  there is no local follow and no whole-queue refresh; link locations are
  text reported unavailable, and a link to the open pull request reloads it.
- **Allocation.** [[BPR-0987]] exact link locations (AC-07, AC-08);
  [[BPR-0988]] the personal discovery read (AC-03, AC-05, AC-06);
  [[BPR-0989]] one discovery scheduler per profile with honest membership
  (AC-02 to AC-06); [[BPR-0990]] Home refresh, discovery status, Follow and
  Save repository with the composed thousands-of-pull-requests course
  (AC-02, AC-04, AC-05, AC-10; answers [[BPR-0801]]). AC-01 and AC-09 need no
  slice and are re-verified at closeout. [[BPR-0599]] depends on all four;
  its planned cards and planned checks are updated, and nothing is claimed
  as run.
- **Gates.** `verify_documentation.py --root .`, `git diff --check` and
  `issue_board validate --rfc RFC-BPR-0026` are recorded on the finish.
  No product code changed.
