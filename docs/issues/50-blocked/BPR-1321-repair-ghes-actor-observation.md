---
schema: "implement-rfc.card.v1"
id: "BPR-1321"
title: "Repair evidence-backed GHES actor observation admission"
status: "Blocked"
rfcs: ["RFC-BPR-0065"]
depends_on: ["BPR-1319"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r4"}
blocked_by: "ghes-actor-provenance"
---

# Repair evidence-backed GHES actor observation admission

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Synthetic attributable actor conflict is repaired at its owning boundary only under established representation/provenance facts; true conflicting observations still reject.
- C2: Composed public PR load, restart/retry and stale completion controls pass with stable actor IDs, exact authority and other relationship validation intact.

## External evidence boundary

Blocked on `ghes-actor-provenance`. BPR-1319 reproduces viewer/review origin
conflicts separately for case-only and different login observations, while
all original relationship rejection remains intact. The private original
exports establish the failure category/counts but omit actor values and their
originating collections. They do not establish which representation is
legitimate. New closed diagnostics identify only origin classes and a case-only
fact, with no login, actor ID, private target or raw response exported.

No normalization or alias rule is inferred from the Git support-policy change.
C1 requires attributable legitimate representation/provenance facts before an
admission repair; C2 requires that repair through the composed PR read path.
Both remain pending. The coordinator owns collection of affected configured
paths/source-build facts and a new read-only failure observation. Source
`e61614205` supplies the privacy-safe diagnostic foundation; combined
`980c08873` repairs the independent Git capability gate and leaves true actor
conflicts rejecting. No private log/content is staged or committed.

## Provenance collection path (dispatch r5, 2026-10-08)

Session `b366df1f04624eb90e90d47c9262e52d` candidate `BPR-1321-pr-loads-v001`
asks whether the originally affected PR loads in the frozen `980c08873`
candidate. On Reject the owner exports an anonymized diagnostic, which carries
only `actor_conflict=<origin>,<origin>,ascii_case_only|different`
(`github_provider.cpp:76-82`, closed export filter in
`release_diagnostic_snapshot.cpp`). That supplies the attributable origin
facts this card is blocked on. An Accept would mean the rejection did not
recur on that PR, which needs its own analysis before any repair. The blocker
stays until one of those results is returned.

## Owner strategy change: affected evidence deferred to pre-release (2026-10-07)

The owner cannot answer the native diagnosis review. Session
`b366df1f04624eb90e90d47c9262e52d` is **withdrawn as unanswerable**: an
objective observation records the withdrawal on each of its three candidates
(cursor 12). No verdict exists; nothing was accepted or rejected. Its server
was stopped and candidate v001 stays protected.

The strategy is now to diagnose from the two existing anonymized diagnostics
logs (private, ignored and never committed), make the most defensible fix and
take affected-installation confirmation from the next pre-release.

**What the logs support.** Every failing load fetched all snapshot pages
(HTTP 200) and failed at the single final validation pass with
`relationship_actor_login`, before the separate timeline read. The search
page is only the last page before validation and contributes no actor. Two
loads in the same window succeeded and later loads failed persistently,
including after restart, which points to new data on the pull request rather
than a transient race. The 0.1.4 build records no actor values, origins or
case-only fact, so the conflicting collections are unknown.

**Defensible repair being made.** Snapshot actors come from REST collections
(viewer, PR, commits, reviews, issue comments) and from GraphQL review-thread
comments, keyed by node ID with a byte-exact login comparison. GitHub
documents different login spellings for App/bot actors across those APIs
(REST `name[bot]`, GraphQL `Bot.login` `name`). The repair canonicalizes only
that typed bot case at the decoding adapter and keeps every other mismatch
rejecting. It is hardening consistent with the facts. It is **not**
attributed to the incident, and confirming it is deferred to pre-release
feedback. C1 and C2 stay open until that feedback or equivalent evidence
exists.
