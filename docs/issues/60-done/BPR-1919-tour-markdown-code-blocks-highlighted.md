---
schema: "implement-rfc.card.v1"
id: "BPR-1919"
title: "Code blocks in rendered tour text and agent answers are syntax highlighted"
status: "Done"
rfcs: []
depends_on: ["BPR-1915"]
priority: "P2"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-06: BPR-1915 look, 'it doesnt look like it was rendering with syntax highlighting'","related_cards":["BPR-1914","BPR-1915"]}
acceptance: [{"criterion": "AC1 A fenced cpp block in a proposal and in a conversation answer renders highlighted, matching the diff view's colours for the same tokens (Qt test on the rendered document's formats)", "outcome": "pass", "revision": "dce73be36", "evidence": {"kind": "command", "detail": "qt_tour_thread_view composes the packaged classifier and the Markdown code-syntax host as the window does: a thread reply's and a proposed explanation's fenced C++ are laid out in at least two colours, the same set as the stop reader gives the same block. qt_tour_agent_session_view: the draft proposal's readers take the same style. The app sets the panes' code colours from the appearance profile (set_code_theme).", "path": "docs/issues/40-parked/BPR-1919-tour-markdown-code-blocks-highlighted.md#verification"}}, {"criterion": "AC2 An unknown or missing language renders plain monospace", "outcome": "pass", "revision": "dce73be36", "evidence": {"kind": "command", "detail": "qt_tour_thread_view: a ```nosuchlanguage block in the same reply keeps one colour.", "path": "docs/issues/40-parked/BPR-1919-tour-markdown-code-blocks-highlighted.md#verification"}}, {"criterion": "AC3 Owner look: Accept/Reject (may share a session with BPR-1914 v002)", "outcome": "pass", "revision": "dce73be36", "evidence": {"kind": "observation", "detail": "The owner accepted BPR-1919 in attended session 2ebe870c371ac8ddd6d5c490add563b5 (candidate BPR-1919-highlighted-code, revision dce73be36-v002), relayed by the coordinator on 2026-11-06.", "path": "docs/issues/60-done/BPR-1919-tour-markdown-code-blocks-highlighted.md#accepted"}}, {"criterion": "AC4 The documentation gate passes", "outcome": "pass", "revision": "dce73be36", "evidence": {"kind": "command", "detail": "verify_documentation.py prints DOCUMENTATION_OK.", "path": "docs/issues/40-parked/BPR-1919-tour-markdown-code-blocks-highlighted.md#verification"}}]
---

# BPR-1919: Code blocks in rendered tour text and agent answers are syntax highlighted

## Owner (2026-11-06)

BPR-1915 accepted, with: "for the formatting one it doesnt look like it was
rendering with syntax highlighting".

## Change

Fenced code blocks (with a language tag, or the file's language where the
block cites a path) in the rendered Markdown of tour proposals, previews and
the tour conversation are highlighted with the app's existing syntax
highlighter, the one the diff view uses, and the same theme in light and dark.
Inline code stays plain monospace. The published GitHub form is unaffected.

## Acceptance

- AC1. A fenced `cpp` block in a proposal and in a conversation answer
  renders highlighted, matching the diff view's colours for the same tokens
  (Qt test on the rendered document's formats).
- AC2. An unknown or missing language renders plain monospace.
- AC3. Owner look: Accept/Reject (may share a session with BPR-1914 v002).
- AC4. The documentation gate passes.

## Implementation (2026-11-06)

`frontend-design:frontend-design` was loaded first.

- The tour editor relays the Markdown code-syntax dispatch it already used
  for the stop reader and previews to the tour agent panel's proposal readers
  and to every thread (BPR-1914 v002), with the profile's code font.
- `review_tour_editor::set_code_theme` gives the stop reader, both previews,
  the proposals and the threads the panes' code colours. The app calls it
  where the appearance is applied (`window_workbench.cpp`), from
  `code_role_colors_of(palette)` and the variant, as the Agent tab and
  comments do, so light and dark follow the diff.
- Replies in the threads are rendered by `markdown_body_view`, so fenced code
  is highlighted by the same path as comments. The secondary conversation
  window's single document stays plain.
- Inline code stays plain monospace. A block in a language the highlighter
  does not know stays plain. The published GitHub form is unchanged.

## Verification

Focused (owner: "dont wait for tests"): `qt_tour_thread_view` (AC1, AC2),
`qt_tour_agent_session_view`, `qt_review_tour_stop_view` and the BPR-1914 v002
set: 18 of 18 pass. `coding-format-check` passes; `verify_documentation.py`
prints `DOCUMENTATION_OK`. The frozen look's stop-code answer shows a
highlighted C++ block under the stop.

## Attended review

Shared with BPR-1914 v002: frozen build `/Users/jaeger/bpr1914-tour-threads-v002.h3uvepo1` of `dce73be36`, launch
`python3 /Users/jaeger/bpr1914-tour-threads-v002.h3uvepo1/launch.py`, session `2ebe870c371ac8ddd6d5c490add563b5`, candidate `BPR-1919-highlighted-code`, its
own Accept/Reject question. Status: ready, awaiting the owner.

BPR-1914 v003 carries this unchanged into its side chats: replies and proposed
texts there render through `markdown_body_view` with the same code style
(`qt_tour_side_chat` repeats AC1 and AC2 on a side chat's reply and proposal).

## Accepted

The owner accepted BPR-1919 on 2026-11-06 (attended review, session
`2ebe870c371ac8ddd6d5c490add563b5`, candidate `BPR-1919-highlighted-code`),
relayed by the coordinator. That session's host is stopped and its frozen
build removed.
