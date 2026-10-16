---
schema: "implement-rfc.card.v1"
id: "BPR-1564"
title: "Markdown reader: suggestion blocks shown as GitHub's Suggested change"
status: "Done"
rfcs: ["RFC-BPR-0036"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","origin":"owner request 2026-10-16: GitHub parity for code blocks in Markdown bodies","governing_rfc":"RFC-BPR-0036","amendment":"RFC-BPR-0036 owner amendment 2 (2026-10-16)","criteria":["AM2-04","AM2-05"],"related_cards":["BPR-1562","BPR-1563","BPR-1212","BPR-1084"],"reservation":"docs/issues/_reservations/BPR-1562--BPR-1564--cc3f094054ee.json","design_skill":"frontend-design:frontend-design"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"425ea6835f10949a99271b3f8b4f0ec8101a5b66","evidence":{"kind":"command","detail":"qt_markdown_code_*: the four BPR-Test#80 suggestions row by row against GitHub (kind, number, text, marked word), no-context added rows, plain outside a line comment, no added control; qt_conversation_cards_view: locator from start line, outdated fallback, PR comment plain; qt_composed_inline_drawer_{100,125,200}: the window's drawer reads the compared head's lines 5-6 (negative control: a locator answering nothing fails); application_github_host: StartLine projected (negative control fails)."}},{"criterion":"AC2","outcome":"pass","revision":"425ea6835f10949a99271b3f8b4f0ec8101a5b66","evidence":{"kind":"command","detail":"Rebased onto main at c9ff3bf50 (BPR-1544 landed); round-trip composition contract resealed (github_application.cpp 686df60d..., production app source ef36523d...); full dev ctest: 762 of 764 passed, then architecture_boundaries passed after the shell guard admitted the exact Linguist alias rows (negative control: a quoted shell elsewhere still fails) and application_inline_publish_cli_consent passed alone (load timing under -j6); application_composition_contract passed; DOCUMENTATION_OK; clang-format 22 and clang-tidy 22 clean on touched lines."}},{"criterion":"AC3","outcome":"pass","revision":"425ea6835f10949a99271b3f8b4f0ec8101a5b66","evidence":{"kind":"observation","detail":"Owner accepted BPR-1564-suggestions-v001 (ea975c2d) with no note in session 03479a9ac4702b758484f2587c7e35f4 on 2026-10-16 (decisions at sequences 13-15, handed back at 16, cursor 16 consumed), on the frozen build from c7aaec454 beside GitHub in Light and Dark."}}]
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
