---
schema: "implement-rfc.card.v1"
id: "BPR-2148"
title: "Suggested prompt macros: chip menu and /p in the popup"
status: "Backlog"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2147"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json","milestone_acceptance":"AC22-AC23"}
---

# Suggested prompt macros: chip menu and /p in the popup

A chip's context menu (Edit this prompt..., Turn off with Undo, Suggested prompts settings...) and prompts in the popup after `/p ` (owner Q6); `/` alone still lists skills only.

## Acceptance

RFC-BPR-0125 AC22-AC23.

## Hygiene audit 2026-11-15

Verdict: **STILL-VALID**. Not built: chips have no context menu ('Edit this prompt', 'Turn off'), and there is no `/p ` popup. Needs refreshing: Edit this prompt should open Settings > Prompts (BPR-2179).
This is a read-only tracker audit: it changes no status, folder or metadata.
