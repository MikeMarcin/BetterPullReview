---
schema: "implement-rfc.card.v1"
id: "BPR-2149"
title: "Suggested prompt macros: share through the exchange"
status: "Backlog"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2146"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC24,AC26"}
---

# Suggested prompt macros: share through the exchange

RFC-BPR-0107's exchange version 2 carries a prompt pack: the reviewer's own
suggested prompts under a pack ID, label and version. Import merges by each
prompt's UUID: Added, Updated in place, conflict (Replace or Keep mine),
Removed from the pack (Remove or Keep) or Unchanged; an older pack is
flagged; prompts not from the pack are never touched. Version 1 files are
unchanged and still written when no prompt is chosen.

Owner amendment, 2026-11-18: "I want to make sure that we can import that
such that it merges in. Aka doesn't replace other different prompts, updates
to the prompt pack can update older imports in-place. This likely means
every prompt needs a stable id (and gets a new one when duplicated)."
Owner decision, the same day: "I'm thinking uuids probably make the most
sense so that we dont get collisions if multiple people all over are making
prompts".

## Acceptance

RFC-BPR-0125 AC24 (as amended 2026-11-18) and AC26.

## Hygiene audit 2026-11-15

Verdict: **STILL-VALID**. Not built: agent_configuration_exchange.cpp carries hooks and the skill but no prompt macros (no version 2).
This is a read-only tracker audit: it changes no status, folder or metadata.
