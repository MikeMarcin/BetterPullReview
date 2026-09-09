---
schema: "implement-rfc.card.v1"
id: "BPR-0710"
title: "Restore exact per-window comparison and semantic reading position"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0709"]
---

# Restore exact per-window comparison and semantic reading position

Dispatch UX-2026-09-08-D2, mandate 1. RFC_BASE_SHA
`e851eb8a851ab605cf5a85260a1317ecdf66773b`; closeout [[BPR-0595]].
Routing: standard | Reason: bounded extension of accepted window restoration.

## Scope and acceptance

Extend the existing profile-private bounded restoration record with the accepted
exact comparison anchors and commit OIDs, optional stable file identity, and
optional exact semantic selection. Never persist a named alias as a substitute
for the accepted pair. Resolve retained identities through the epoch/cache host;
missing/pruned epochs, removed files, changed semantic material and unavailable
content report failure without selecting another comparison or line.

Preserve independent stable window identities and accepted-only updates. PR
replacement clears its prior reading position; same-PR refresh and failed
restoration retain it. Home, newer navigation, identity replacement and Close
defeat late restoration. Quit drains accepted writes. Schema migration preserves
version-one PR-only records, revision authority and transactional interruption
behavior; corrupt/future records stay intact and visibly unavailable.

Exercise actual-process restart with two windows holding different exact pairs,
files and semantic locations through the real installed-Git diff path. Include
unavailable content and cancellation courses, direct core/host/storage checks,
changed-TU format/tidy, production composition checks and one integration pass.
An independent persistence/identity review assesses the bounded candidate.

The anonymous BPR-0709 fixture's existing unselected-comparison limitation is
not passing comparison evidence. Native geometry, full RFC/native criteria,
cross-process admission, same-review ledger fanout and complete two-account
isolation remain later obligations. RFC-0028 Home/layout authority stays in
force; browser-style file tabs remain unscheduled.

Owned paths: restoration core/host/storage, narrow existing epoch/workbench
composition and Qt semantic projection seam if required, their focused tests,
build declarations and owning architecture/closeout documentation. No live
GitHub mutation, credentials, push or publication.
