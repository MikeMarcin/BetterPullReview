---
schema: "implement-rfc.card.v1"
id: "BPR-1795"
title: "A tab in a list or heading sends the whole body to source"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1793--BPR-1795--a53cc4b7106f.json","kind":"bug","found_by":"BPR-1793 cmark-gfm comparison, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: []
---

# BPR-1795: A tab in a list or heading sends the whole body to source

Found by [BPR-1793](../60-done/BPR-1793-evaluate-cmark-gfm-for-the-markdown-reader.md).
`restore_code_tabs` refuses any tab-bearing construct it cannot map back to
the source, and the reader then shows the whole body as source ("This
tab-bearing construct cannot be rendered without changing whitespace").
All 15 such examples hold a tab: the spec's Tabs section, a tab-only blank
line inside a list, a tab after a setext underline, a trailing tab in a
heading. GitHub renders all of them.

## Acceptance

- AC1. A tab outside code is read as CommonMark reads it, and the body
  renders; a tab inside code keeps its width exactly as today; the fallback
  remains only where code whitespace truly cannot be kept.
