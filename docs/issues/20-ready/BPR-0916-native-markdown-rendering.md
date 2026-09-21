---
schema: "implement-rfc.card.v1"
id: "BPR-0916"
title: "Implement native Markdown rendering for review content"
status: "Ready"
rfcs: ["RFC-BPR-0036"]
depends_on: []
milestone: "Post-Pack v2"
metadata: {"milestone_plan":{"id":"rfc0036-native-markdown","path":"docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}}
---

# Implement native Markdown rendering for review content

Owner feedback after the first loaded PR: “We need an RFC to support markdown
rendering.” Implementation is now authorized by the owner's request to implement
RFC-BPR-0036 in a worktree (mandate r1, 2026-09-20).

Follow [RFC-BPR-0036](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md).
Before source work, inventory overview and public-conversation body paths,
select the supported dialect and native renderer, define measured processing
bounds, and split bounded implementation slices as needed. Preserve original
Markdown for source/copy/publication and keep all resource/navigation effects
out of widgets. No embedded browser or automatic remote image loading.

## Acceptance

Implement AC-01 through AC-06 and record direct composed evidence on [[BPR-0917]].
Coordinate long-description geometry with [[BPR-0918]] without blocking that
investigation on renderer implementation. Use synthetic bodies covering long
text, nested structures, wide tables/code and hostile resource references.

- M1: Inventory the initial surfaces, document the selected dialect, resource
  policy and measured bounds, and implement the shared native component with
  source/copy, selection, keyboard links and synthetic-corpus verification.
- M2: Integrate overview and public conversations through the shipped
  composition, route admitted navigation through host policy, preserve original
  bodies and long-description geometry, and supply AC-01 through AC-06 evidence.

## Status

Milestone 1 is starting; this card remains open until both delivery stages pass.
