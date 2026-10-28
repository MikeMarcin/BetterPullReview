---
schema: "implement-rfc.card.v1"
id: "BPR-1795"
title: "A tab in a list or heading sends the whole body to source"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1793--BPR-1795--a53cc4b7106f.json","kind":"bug","found_by":"BPR-1793 cmark-gfm comparison, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "78db562dcd2d2924f66d24cd4530a6bb429cc5df", "evidence": {"kind": "command", "detail": "qt_markdown_body_1_0, _1_25, _2_0 check_bounds: ordinary\ttab, a tab-only blank line in a list, a tab after a setext underline and the spec's indented-code tab line render, and ordinary\ttab keeps its tab; a quoted fence with a leading tab and an indented code line starting with tabs still fall back with the full source. qt_markdown_code unchanged. qt_markdown_cmark_spec: 12 of the 15 tab fallbacks now render and match; the 3 left are listed as tab-fallback."}}]
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

## Built

- Qt 6.12 keeps tabs inside code text and expands only a code line's leading
  tabs, which the existing repair restores for top-level fences. A tab
  outside those fences no longer refuses the body: md4c reads it as
  CommonMark does. Only an indented code block whose text starts with
  whitespace, where an expanded tab cannot be told from authored spaces,
  still keeps the source view.
- Found by the same comparison and fixed with it:
  - Qt's GitHub dialect also reads YAML front matter (`0x100000`); a body
    opening with `---` lines lost them. Comments and descriptions have no
    front matter on GitHub, so that feature is off.
  - The soft-break mark was added inside an inline link's `](…)` and on a
    link reference definition's continuation lines, which broke the link or
    put the mark in its address. Both are skipped now.
  - A lone CR is a line ending (`\r\r\n` is two).

## Not done

- The three spec examples where a tab is partly used as indentation in
  indented code still keep the source view.
