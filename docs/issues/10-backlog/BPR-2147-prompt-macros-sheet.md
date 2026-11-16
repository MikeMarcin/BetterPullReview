---
schema: "implement-rfc.card.v1"
id: "BPR-2147"
title: "Suggested prompt macros: the editor sheet"
status: "Backlog"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2146"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC14-AC21"}
---

# Suggested prompt macros: the editor sheet

Add, edit, duplicate, delete with Undo, reset; Insert and the `{` list; variable tint; preview and Shows now; condition pickers; every validation message; keyboard and accessibility; then the owner's look at a frozen build (roster, sheet, chips, sent turn).

## Acceptance

RFC-BPR-0125 AC14-AC21.

## Hygiene audit 2026-11-15

Verdict: **SUPERSEDED**. By the Prompts section, BPR-2179, BPR-2185 and BPR-2209 (Done; owner looks prompts-v001 to v004 accepted). prompts_page.cpp has Duplicate (:992), Undo (:889), Insert (:1765) and a byte count (:1788). RFC-BPR-0125 AC14-AC21 still describe the earlier sheet (for example 16 KiB against the built 8,192 bytes), so BPR-2150 needs them remapped.
This is a read-only tracker audit: it changes no status, folder or metadata.
