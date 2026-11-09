---
schema: "implement-rfc.card.v1"
id: "BPR-1963"
title: "Tour fixture owns the source it copies while growing its stop vector"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind": "test-correctness", "found_by": "Windows43 diagnosis", "reservation": "docs/issues/_reservations/BPR-1963--BPR-1963--2a07c806134b.json", "milestone_plan": {"id": "bpr-1963-tour-fixture-value-lifetime", "path": "docs/rfcs/plans/bpr-1963-tour-fixture-value-lifetime/milestone-plan.json"}}
---

# BPR-1963: Tour fixture owns its source across vector growth

## Acceptance

- AC1. Copy the initial Original source by value before pushing more stops; all constructed content, gestures, oracles and production behavior remain.
- AC2. Isolated one-TU AddressSanitizer negative/positive proof and normal Release/Debug registered qt_tour_agent_session_view pass after correction; retain exact negatives and make no unsupported Windows causal claim.
- AC3. Focused format/tidy/docs/board and intentional latest-main integration pass; exact source/binary/command ownership and pending full native qualification are explicit.

## Diagnosis

whole_tour holds a reference into tour.Stops.front().Original, then pushes three more stops. The first vector reallocation invalidates that reference and the next copy uses freed storage. The source template must be an owned value. Coordinator assigned only this test file; frozen producers remain untouched.
