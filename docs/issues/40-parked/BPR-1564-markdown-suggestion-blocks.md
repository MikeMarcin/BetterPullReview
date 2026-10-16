---
schema: "implement-rfc.card.v1"
id: "BPR-1564"
title: "Markdown reader: suggestion blocks shown as GitHub's Suggested change"
status: "Parked"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-04","AM2-05"],"related_cards":["BPR-1562","BPR-1563","BPR-1212","BPR-1084"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"command","detail":"qt_markdown_code_*: the four BPR-Test#80 suggestions row by row against GitHub (kind, number, text, marked word), no-context added rows, plain outside a line comment, no added control; qt_conversation_cards_view: locator from start line, outdated fallback, PR comment plain; qt_composed_inline_drawer_{100,125,200}: the window's drawer reads the compared head's lines 5-6 (negative control: a locator answering nothing fails); application_github_host: StartLine projected (negative control fails)."}},{"criterion":"AC2","outcome":"pending","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"freeform","detail":"Pending at landing: rebase onto main after BPR-1544, reseal the round-trip composition contract over the merged tree, full dev ctest once, documentation gate. Done so far: clang-format 22 and clang-tidy 22 clean on touched files (main.cpp's remaining findings are on untouched lines), DOCUMENTATION_OK, 50 related rows plus qt_composed_inline_drawer_* pass."}},{"criterion":"AC3","outcome":"pending","revision":"f1b2c064c5f54f397e285209a5fe83ef508e9d52","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in session 03479a9ac4702b758484f2587c7e35f4 (state /Users/jaeger/.mask/attended-review-sessions/bpr-1562-fenced-code-v001-20261016, cursor 12): candidate BPR-1564-suggestions-v001 (ea975c2d...); frozen root /Users/jaeger/bpr-1562-fenced-code-v001.lmt9brah from c7aaec454 (executable 73e6b117...); GitHub-vs-app sheets in Light and Dark; live sample MikeMarcin/BPR-Test#80."}}]
---

# BPR-1564: Markdown reader: suggestion blocks shown as GitHub's Suggested change

Under [RFC-BPR-0036 owner amendment 2](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md#owner-amendment-2--2026-10-16-fenced-code-as-github-draws-it),
a `suggestion` block in a published line comment reads as GitHub's
**Suggested change**. GitHub's own view was read from a review on
MikeMarcin/BPR-Test#80: the commented lines all as removed rows, then the
block's lines all as added rows (no context rows), both numbered from the
first commented line, changed words marked only on equal-count pairs, the
code coloured by the file's language.

## Acceptance

- **AC1 (AM2-04):** a current single-line, multi-line, inserting and deleting
  suggestion show GitHub's rows, numbers and word marks; an outdated thread,
  a thread on another file or on the base side shows the added rows only; a
  description, issue comment or review summary shows plain code; there is no
  apply action.
- **AC2:** gates and the reader's rows, as on [[BPR-1562]].
- **AC3 (AM2-05), owner:** Accept/Reject beside GitHub in Light and Dark.

## Design and evidence

Shared with [[BPR-1562]] (design and evidence there). GitHub's box read from
BPR-Test#80 (`code-blocks-v1/suggestions.github.html`): a 1 px rule and 6 px
corners, the "Suggested change" header in the quiet ink, full-width rows in
the diff fills with a darker number gutter, changed words on the intraline
fill, code in the file's language. Commented lines come from the compared
document (`core::suggestion_source_lines`, from GitHub's `start_line`, now in
`github_conversation_projection`) while the thread is current on that file;
otherwise the added rows alone. Read only: RFC-BPR-0048 keeps apply as a
GitHub handoff, and no native apply exists. `qt_markdown_code_*` compares
the four review suggestions row by row with GitHub (kind, number, text,
marked word); `qt_conversation_cards_view` drives the drawer's locator.
