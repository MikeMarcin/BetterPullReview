---
schema: "implement-rfc.card.v1"
id: "BPR-0562"
title: "Implement RFC-BPR-0022 core publication placement policy"
status: "Done"
rfcs: ["RFC-BPR-0022"]
depends_on: ["BPR-0561"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0022-off-hunk-comments","path":"docs/rfcs/plans/rfc-bpr-0022-off-hunk-comments/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1bc5b10984753ac933ceeae76a0f4e3c5f37faf3","evidence":{"kind":"command","detail":"Focused core decision/transition tests and both architecture gates pass in dev and core-only (3/3 each); source contract documented in review-round-trip.md.","command":"ctest --preset core-only -R ^(core_review_round_trip|architecture_boundaries|architecture_boundary_contract)$ --output-on-failure"}},{"criterion":"AC2","outcome":"pass","revision":"1bc5b10984753ac933ceeae76a0f4e3c5f37faf3","evidence":{"kind":"command","detail":"Focused core decision/transition tests and both architecture gates pass in dev and core-only (3/3 each); source contract documented in review-round-trip.md.","command":"ctest --preset core-only -R ^(core_review_round_trip|architecture_boundaries|architecture_boundary_contract)$ --output-on-failure"}},{"criterion":"AC3","outcome":"pass","revision":"1bc5b10984753ac933ceeae76a0f4e3c5f37faf3","evidence":{"kind":"command","detail":"Focused core decision/transition tests and both architecture gates pass in dev and core-only (3/3 each); source contract documented in review-round-trip.md.","command":"ctest --preset core-only -R ^(core_review_round_trip|architecture_boundaries|architecture_boundary_contract)$ --output-on-failure"}},{"criterion":"AC4","outcome":"pass","revision":"1bc5b10984753ac933ceeae76a0f4e3c5f37faf3","evidence":{"kind":"command","detail":"Focused core decision/transition tests and both architecture gates pass in dev and core-only (3/3 each); source contract documented in review-round-trip.md.","command":"ctest --preset core-only -R ^(core_review_round_trip|architecture_boundaries|architecture_boundary_contract)$ --output-on-failure"}},{"criterion":"AC5","outcome":"pass","revision":"1bc5b10984753ac933ceeae76a0f4e3c5f37faf3","evidence":{"kind":"command","detail":"Focused core decision/transition tests and both architecture gates pass in dev and core-only (3/3 each); source contract documented in review-round-trip.md.","command":"ctest --preset core-only -R ^(core_review_round_trip|architecture_boundaries|architecture_boundary_contract)$ --output-on-failure"}}]
---

# Implement RFC-BPR-0022 core publication placement policy

Add provider-free vocabulary and pure policy for semantic source eligibility, placement evidence, off-hunk capability, publication representation, body compatibility, legal fallback authorization, and typed diagnostics.

## Acceptance

- The decision table covers legacy/off-hunk/unknown placement × supported/unsupported/unknown capability.
- Unknown placement chooses native negotiation, never inferred fallback.
- Fallback requires the live RFC's durable exact rejection or no-native-attempt
  admission with fresh scoped unsupported evidence and proven off-hunk placement.
- Line-only body semantics produce an explicit incompatibility when fallback would be required.
- Core tests run without Qt, HTTP, filesystem, clock, thread, database, or provider types.

## Verification

Record the focused command or attended interaction, result, durable evidence,
and producing commit on the live card. Reuse passing evidence only when its
inputs did not change. Run composition-oriented selectors for any slice that
changes the shipped path.

## Import boundary

Imported 2026-09-04 under an import-only direction. The 2026-09-26
implementation mandate r1 activates RFC-BPR-0022; live dependencies and lanes
remain authoritative. Follow the live RFC and [import reconciliation](../../rfcs/assets/betterpullreview-rfc-bpr-0022-off-hunk-comments-v1.LIVE_IMPORT.md),
which supersede conflicting source-pack authorization and contract wording.

Cover both first-attempt unsupported-evidence admission and exact native-rejection
negotiation. Persist the authorization before a fallback child exists; never
fabricate a rejected native child or authorize fallback from a lost response.

## Acceptance evidence — 2026-09-26

Source candidate `1bc5b10984753ac933ceeae76a0f4e3c5f37faf3`. `core_review_round_trip` exercises every
placement/capability cell, stale capability, both durable fallback admissions,
indeterminate-native refusal, terminal receipt replay and line-only incompatibility.
Dev and core-only each pass the focused core test plus `architecture_boundaries`
and `architecture_boundary_contract` (3/3). Core-only compiles without Qt/provider
dependencies. Source self-review preserves native prepared work when cache
evidence changes, forbids fallback from a witness, and treats suggestions with
spaced/longer fences conservatively. The planner is not yet wired to provider
recovery; that belongs to authored milestone B. Documentation and whitespace pass.
