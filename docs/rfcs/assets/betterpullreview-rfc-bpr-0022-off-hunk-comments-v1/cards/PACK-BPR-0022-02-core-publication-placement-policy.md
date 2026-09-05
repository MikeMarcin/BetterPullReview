---
provisional_id: PACK-BPR-0022-02
live_id: allocate-on-import
title: Implement RFC-BPR-0022 core publication placement policy
status: Backlog
rfc: RFC-BPR-0022
depends_on: [PACK-BPR-0022-01]
---

# Implement RFC-BPR-0022 core publication placement policy

Add provider-free vocabulary and pure policy for semantic source eligibility, placement evidence, off-hunk capability, publication representation, body compatibility, legal fallback authorization, and typed diagnostics.

## Acceptance

- The decision table covers legacy/off-hunk/unknown placement × supported/unsupported/unknown capability.
- Unknown placement chooses native negotiation, never inferred fallback.
- Only a definitive unsupported observation permits the fallback transition.
- Line-only body semantics produce an explicit incompatibility when fallback would be required.
- Core tests run without Qt, HTTP, filesystem, clock, thread, database, or provider types.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.
