---
schema: "implement-rfc.card.v1"
id: "BPR-1794"
title: "Double-underscore strong emphasis renders as italic"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1793--BPR-1795--a53cc4b7106f.json","kind":"bug","found_by":"BPR-1793 cmark-gfm comparison, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "78db562dcd2d2924f66d24cd4530a6bb429cc5df", "evidence": {"kind": "command", "detail": "qt_markdown_github_1_0, _1_25, _2_0 check_underscores: __text__ bold not italic, _text_ italic, __*text*__ both, snake_case_name stays literal, <ins> still the only underline; check_breaks' emphasis still closes. qt_markdown_cmark_spec: the 14 emphasis examples now match cmark-gfm."}}]
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

## Built

Qt passes its Markdown features to md4c as parser flags, and its GitHub
dialect sets md4c's underline extension (`0x4000`). The reader now imports
without it, so md4c reads `_text_` and `__text__` by CommonMark's own rules,
and `present_underscore_emphasis`, which turned every underline italic, is
gone. `<ins>` still underlines.
