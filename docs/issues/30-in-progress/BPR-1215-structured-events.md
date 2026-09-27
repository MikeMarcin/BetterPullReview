---
schema: "implement-rfc.card.v1"
id: "BPR-1215"
title: "Version bounded diagnostic events as JSONL"
status: "In Progress"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-0602"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
---

# Version bounded diagnostic events as JSONL

The shipped logger writes bounded versioned JSONL with stable event/category, severity, UTC and monotonic time, opaque session and sequence, build identity and existing operation correlation. Existing metadata export reads the new records and legacy records without copying free text.

## Acceptance

- AC1: Persist parseable bounded structured records with explicit event identity, times, session, sequence, build and existing typed facts.
- AC2: Preserve BPR-0971 sanitizer and fail-closed malformed input, loss and file boundaries.
- AC3: Exercise provider-to-file public composition and existing diagnostic export; update owning docs.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
