---
schema: "implement-rfc.card.v1"
id: "BPR-2147"
title: "Suggested prompt macros: the editor sheet"
status: "Done"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2146"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC14-AC21","superseded_by":["BPR-2179","BPR-2185","BPR-2209"],"owner_closure":{"date":"2026-11-16","source":"owner decisions on the 2026-11-15 hygiene audit, relayed by the coordinator","decision":"close as superseded"}}
---

# Suggested prompt macros: the editor sheet

Add, edit, duplicate, delete with Undo, reset; Insert and the `{` list; variable tint; preview and Shows now; condition pickers; every validation message; keyboard and accessibility; then the owner's look at a frozen build (roster, sheet, chips, sent turn).

## Acceptance

RFC-BPR-0125 AC14-AC21.

## Hygiene audit 2026-11-15

Verdict: **SUPERSEDED**. By the Prompts section, BPR-2179, BPR-2185 and BPR-2209 (Done; owner looks prompts-v001 to v004 accepted). prompts_page.cpp has Duplicate (:992), Undo (:889), Insert (:1765) and a byte count (:1788). RFC-BPR-0125 AC14-AC21 still describe the earlier sheet (for example 16 KiB against the built 8,192 bytes), so BPR-2150 needs them remapped.
This is a read-only tracker audit: it changes no status, folder or metadata.

## Owner decision 2026-11-16: closed as superseded

Owner, 2026-11-16 (on the hygiene audit, relayed by the coordinator): close
this card as superseded. The board has one terminal lane, so a superseded card
sits in Done with `metadata.superseded_by` (as [[BPR-1353]] does); it was not
delivered as written.

By the Prompts section of Settings: [[BPR-2179]], [[BPR-2185]] and
[[BPR-2209]] (Done; owner looks prompts-v001 to prompts-v004 accepted).
`prompts_page.cpp` has the editor this card planned: Duplicate, Undo,
Insert and a byte count. RFC-BPR-0125's AC14 to AC21 still describe the
earlier sheet (for example a 16 KiB limit where the built page holds 8,192
bytes), so its closeout [[BPR-2150]] has to remap them to what was built;
that is the closeout's work, not this card's. [[BPR-2148]] and
[[BPR-2150]] depended on this card; the editor they need exists.
