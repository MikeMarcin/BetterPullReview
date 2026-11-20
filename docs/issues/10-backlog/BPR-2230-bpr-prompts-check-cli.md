---
schema: "implement-rfc.card.v1"
id: "BPR-2230"
title: "bpr prompts check FILE: validate an exported prompt file before import"
status: "Backlog"
rfcs: ["RFC-BPR-0129"]
depends_on: ["BPR-2229"]
priority: "P3"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2229--BPR-2230--95323aae2ed0.json","milestone":"M2","milestone_acceptance":"AC21"}
---

# bpr prompts check FILE: validate an exported prompt file before import

RFC-BPR-0129 ([RFC](../../rfcs/RFC-BPR-0129-prompts-declare-a-result-and-features.md)); optional, owner decision 2026-11-16. An agent
that edits an exported file runs this to learn what import would refuse.

## Scope

- `bpr prompts check FILE [--profile NAME]`, in the existing `bpr` CLI
  beside `bpr tour` and `bpr draft`.
- It prints [[BPR-2229]]'s core decisions and reasons for each prompt.
  Without `--profile` it checks against the catalog alone. With it, it
  also says Add or Change against that profile.
- It exits non-zero if any prompt would be refused. It writes nothing,
  either to the profile or to the file.

## Acceptance

RFC-BPR-0129 AC21.
