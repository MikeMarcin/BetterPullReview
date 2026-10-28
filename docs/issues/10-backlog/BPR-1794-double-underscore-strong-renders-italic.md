---
schema: "implement-rfc.card.v1"
id: "BPR-1794"
title: "Double-underscore strong emphasis renders as italic"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1793--BPR-1795--a53cc4b7106f.json","kind":"bug","found_by":"BPR-1793 cmark-gfm comparison, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: []
---

# BPR-1794: Double-underscore strong emphasis renders as italic

Found by [BPR-1793](../60-done/BPR-1793-evaluate-cmark-gfm-for-the-markdown-reader.md).
`__foo bar__` reads as italic; GitHub draws it bold. Qt's GitHub dialect
(md4c with its underline extension) imports both `_x_` and `__x__` as
underline, and `present_underscore_emphasis` turns every underline into
italic, so the strong form cannot be told apart after the import. The
CommonMark spec's emphasis examples 391, 398, 399, 411, 412 and 433 to 435
show it; 14 examples in all.

## Acceptance

- AC1. `__x__` is bold and `_x_` italic, by CommonMark's delimiter rules
  (intraword underscores stay literal), in the GitHub parity course;
  `**`/`*` unchanged; code spans untouched.
