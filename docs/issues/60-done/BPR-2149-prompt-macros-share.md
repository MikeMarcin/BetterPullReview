---
schema: "implement-rfc.card.v1"
id: "BPR-2149"
title: "Suggested prompt macros: share through the exchange"
status: "Done"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2146"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC24,AC26"}
acceptance: [{"criterion":"AC24","outcome":"pass","revision":"19918b9a7","evidence":{"kind":"command","detail":"core_prompt_pack (every planning case: added, updated in place keeping On and placements, conflict Replace/Keep mine, unchanged, dropped Remove/Keep, older flagged, outside prompts untouched; UUID refusal; Duplicate drops the link; v1 without prompts, v2 with a pack, v3 refused); sqlite_prompt_pack_storage (export records links and renames a pre-UUID prompt, concurrent edit refuses, table made on an older store); qt_agent_configuration_dialogs (export pack choice, import preview standings and choices); core_agent_configuration, application_prompt_macro_host, application_ask_menu_public_path"}},{"criterion":"AC26","outcome":"pass","revision":"19918b9a7","evidence":{"kind":"command","detail":"core_prompt_pack test_round_trip and sqlite_prompt_pack_storage: export, import into a clean profile adds every prompt, re-import of version 2 updates in place, the recipient's own prompt and Off unchanged"}}]
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

## Done (2026-11-18)

Built at `19918b9a7` on `claude/v0-5-0-prerelease-targets-52265d`. The owner
waived a look: "No review on this one just make it work and land it on main".

- Core `bpr/core/prompt_pack.hpp`: pack identity, portable prompts, plan and
  all-or-none apply, export with links; `prompt_macro::Pack` carries the link,
  `duplicate_prompt` drops it.
- Exchange: version 2 only with a pack; version 1 unchanged and still read.
- Host: own prompt IDs are version 4 UUIDs (`mint_prompt_uuid`); the exchange
  snapshot carries the prompts and their revision.
- SQLite: `prompt_macro_packs`, made where a store lacks it.
- Qt: Export's Suggested prompts and pack choice; Import's pack section.

Decisions recorded in the RFC: built-ins are not exported (duplicate one to
share it); a prompt made before UUIDs gets one on first export, not by a store
migration; a conflict starts at Keep mine; a dropped prompt starts at Remove
unless edited; an older pack starts with updates and removals unchosen.
