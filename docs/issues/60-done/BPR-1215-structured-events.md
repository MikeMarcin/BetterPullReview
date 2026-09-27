---
schema: "implement-rfc.card.v1"
id: "BPR-1215"
title: "Version bounded diagnostic events as JSONL"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-0602"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"ac64e5ad0dc4694dad4c6db3a40f12479b11503e","evidence":{"kind":"observation","detail":"Independent Qt JSON parser verifies actual file schema, UTC/elapsed time, session/sequence, build identity, severity and maximum escaped summary; application_logging validates the shipped composition.","reference":"docs/verification/RFC-BPR-0027.md"}},{"criterion":"AC2","outcome":"pass","revision":"ac64e5ad0dc4694dad4c6db3a40f12479b11503e","evidence":{"kind":"observation","detail":"quill_logging, content_policy, backpressure and application full/unavailable cases pass; malformed/control/UTF8/duplicate/partial inputs are refused or omitted without passthrough.","reference":"docs/verification/RFC-BPR-0027.md"}},{"criterion":"AC3","outcome":"pass","revision":"ac64e5ad0dc4694dad4c6db3a40f12479b11503e","evidence":{"kind":"observation","detail":"12/12 integration checks pass, including public maintenance and profile/multiwindow courses; documentation and 64 documentation tests pass; 9-file format check and 2-TU tidy pass.","reference":"docs/verification/RFC-BPR-0027.md"}}]
---

# Version bounded diagnostic events as JSONL

The shipped logger writes bounded versioned JSONL with stable event/category, severity, UTC and monotonic time, opaque session and sequence, build identity and existing operation correlation. Existing metadata export reads the new records and legacy records without copying free text.

## Acceptance

- AC1: Persist parseable bounded structured records with explicit event identity, times, session, sequence, build and existing typed facts.
- AC2: Preserve BPR-0971 sanitizer and fail-closed malformed input, loss and file boundaries.
- AC3: Exercise provider-to-file public composition and existing diagnostic export; update owning docs.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
