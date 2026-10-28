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
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "0ce8b879c71f121786d4ce41f9d516d9198cf0ae", "evidence": {"kind": "command", "detail": "Scratch harness (not committed) fed every example of cmark-gfm 27d942c8b's spec.txt (670), extensions.txt (30) and regression.txt (26) through markdown_body_view and, for the expected side, cmark-gfm's expected HTML through QTextDocument::setHtml, reducing both to block kind and styled runs. cmark-gfm passes its own suites (670 and 30)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "1a43b807d7157626d760a16eae43b0c01af5aa40", "evidence": {"kind": "command", "detail": "qt_markdown_cmark_spec: 726 examples read, 604 match cmark-gfm, 122 listed in known-differences.json with a category and reason; a negative control (one listed difference removed, one matching example added) fails with NEW DIFFERENCE and NOW MATCHES."}}]
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

## Follow-up, 2026-10-27: fixes and a regression course

The owner asked for the defects to be fixed and the comparison kept as a
regression test if its licences allow.

- Fixed: BPR-1794 (`__strong__`) and BPR-1795 (tabs), and with them Qt's
  front matter hiding a body's opening `---` lines, the soft-break mark
  inside multi-line links and link reference definitions, and a lone CR.
  Matches across the three suites went from 569 to 604 of 726; nothing that
  matched before stopped matching.
- Then GFM's autolinks, BPR-1796: 607 of 726 match, 119 listed.

### The course

`qt_markdown_cmark_spec` (`tests/adapters_qt/markdown_cmark_spec_tests.cpp`)
reads every example of the three suites with the reader and compares it
with cmark-gfm's expected HTML, as this evaluation did. It is a ratchet:
`known-differences.json` lists each of the 122 known differences by
category with a reason (images, sanitized HTML, footnotes, the HTML
import's quote levels, link encoding and md4c's own table, autolink,
entity and strikethrough rules). A new difference fails the course, and so
does a listed example that now matches. `BPR_CMARK_SPEC_DUMP=<file>` writes
each difference's shapes for classifying.

### Licences

The suites live in `tests/fixtures/markdown/cmark-gfm-spec-v1/`, copied
unmodified with a provenance record (source commit, SHA-256 of each file):
`spec.txt` and `extensions.txt` are CC BY-SA 4.0, `regression.txt` is
covered by cmark-gfm's BSD-2 `COPYING`, kept beside it. This causes no
trouble: they are test data that one test reads at run time, never built
from, linked or installed, and no part of them is copied into another file.
Share-alike binds only those files, which stay unchanged with their
attribution, so it reaches neither our code nor `known-differences.json`
(example names and our reasons). The source archive carries them with
their notices. They are not runtime components, so they are not in
`third_party/` or the installed notices.
