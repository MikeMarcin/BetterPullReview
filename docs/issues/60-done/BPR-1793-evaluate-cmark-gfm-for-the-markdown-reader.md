---
schema: "implement-rfc.card.v1"
id: "BPR-1793"
title: "Evaluate cmark-gfm for the Markdown reader"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1793--BPR-1795--a53cc4b7106f.json","kind":"evaluation","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "0ce8b879c71f121786d4ce41f9d516d9198cf0ae", "evidence": {"kind": "command", "detail": "Scratch harness (not committed) fed every example of cmark-gfm 27d942c8b's spec.txt (670), extensions.txt (30) and regression.txt (26) through markdown_body_view and, for the expected side, cmark-gfm's expected HTML through QTextDocument::setHtml, reducing both to block kind and styled runs. cmark-gfm passes its own suites (670 and 30)."}}]
---

# BPR-1793: Evaluate cmark-gfm for the Markdown reader

## Request

The owner, 2026-10-27: evaluate cmark-gfm, "even if just to compare its test
suite against our parser", and whether to adopt it or keep the current
setup. (PlutoBook was set aside: it lays out HTML and CSS for print and
images, not selectable, accessible text in a Qt view.)

## Earlier decision

`docs/architecture/native-markdown.md` compared only Qt's native import
(md4c) with md4qt and chose Qt's. cmark-gfm was never evaluated.

## The candidate

`github/cmark-gfm` at `27d942c8b` (2026-09-28), GitHub's own parser: C,
BSD-2 with bundled parts under their own notices (GitHub shows the licence
as NOASSERTION). Active but slow: last release `0.29.0.gfm.13` (July
2023), 137 open issues. It builds in under a second: a 387 KB library plus
47 KB of extensions. It gives a syntax tree with source positions
(`--sourcepos`), has a pathological-input suite and a fuzz target, and
parsed and wrote HTML for a 64 KiB body in under 10 ms. The spec text is
CC BY-SA 4.0.

## Comparison

The harness compares document shape: block kind (heading level, list and
depth, quote level, code, table cell, rule, task marker) and runs (bold,
italic, strike, code, link target), with whitespace and line separators
folded. It is approximate: Qt's HTML import has its own quirks (it puts a
block quote's level on the wrong blocks), so a mismatch is a lead, not a
verdict.

| Suite | Examples | Our reader matches | Qt import alone matches |
| --- | --- | --- | --- |
| spec | 670 | 538 (80%) | 470 (70%) |
| extensions | 30 | 16 (53%) | 11 (36%) |
| regression | 26 | 15 (57%) | 14 (53%) |

Most of the remaining mismatches are intended or are harness noise:

- Images (18): the reader shows an accessible placeholder until images are
  allowed (RFC-BPR-0084).
- Raw HTML and HTML blocks (25): the spec passes HTML through; GitHub and
  the reader sanitize it (BPR-1544).
- Footnotes and task lists: the reader numbers footnote targets and draws
  bracketed references and native task markers, which the expected HTML
  lacks.
- Block quotes and nested lists: mostly the HTML import's quote levels.

Real defects it found:

- **`__strong__` reads as italic** (14 examples). Qt's dialect turns both
  `_x_` and `__x__` into underline, and `present_underscore_emphasis`
  makes every underline italic, so the strong form is lost. BPR-1794.
- **A tab makes the whole body fall back to source** (15 examples, all with
  a tab): `restore_code_tabs` refuses any tab-bearing construct it cannot
  map, even a tab-only blank line inside a list. BPR-1795.
- Smaller: a bare URL with non-ASCII characters is not autolinked; a link
  destination with a space is not percent-encoded; `\r\r\n` joins two
  lines.

## Recommendation: keep the current setup

Adopting cmark-gfm means writing our own syntax-tree-to-`QTextDocument`
builder in place of Qt's import, then re-qualifying everything the reader
already does on top of it: code tabs and highlighting, alerts, `<details>`,
the HTML subset, footnotes, images, suggestions, selection, accessibility
and source mapping. It gains exact GFM parsing and source positions; the
parity gap that matters is two fixable defects, not the parser. Parsing is
not the reader's cost: layout is.

Do instead:

- Fix BPR-1794 and BPR-1795.
- Revisit cmark-gfm if source positions become needed (precise
  rendered-to-source mapping would remove the tab fallback's cause), or if
  a later comparison shows parsing gaps the preparation passes cannot close.
- Optionally keep the comparison as a nongating course, with the spec
  acquired pinned at test time rather than vendored (CC BY-SA 4.0).
