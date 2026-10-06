---
schema: "implement-rfc.card.v1"
id: "BPR-1298"
title: "Desktop draft and reply editors read CRLF back as LF"
status: "Ready"
rfcs: ["RFC-BPR-0048"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"found_by":"BPR-1084 implementation, bpr-main-linux, 2026-10-06","reservation":"docs/issues/_reservations/BPR-1298--BPR-1298--7914bd7bf76b.json","related_cards":["BPR-1084","BPR-1290","BPR-1083"],"acceptance":["BPR-1298:round-trip","BPR-1298:edits","BPR-1298:controls"]}
---

# BPR-1298: Desktop draft and reply editors read CRLF back as LF

## Observation

Found while implementing [[BPR-1084]] on `bpr-main-linux`. The desktop
comment draft editor reads its body through `QPlainTextEdit::toPlainText()`.
[[BPR-1290]]'s in-place reply editor mirrors that editor. A Qt plain-text
editor stores each line break as a paragraph separator and reads every break
back as LF.

A draft body with CRLF endings, written by `bpr cycle` or an agent, therefore
lost its CRs at its first desktop edit, a label-chip or shortcode edit
included. The core keeps CRLF (RFC-BPR-0048 R12), so the loss was only in the
GUI path.

## Outcome

The desktop editors keep a body's line endings exactly on a round trip. That
covers label-chip, toggle and shortcode edits, which are the label controls'
typed header edits. A CRLF body written by the command line or an agent keeps
its CRs.

## Acceptance

- **BPR-1298:round-trip.** A presented body reads back byte for byte, mixed
  endings included, and presenting it sends nothing.
- **BPR-1298:edits.** A relabel or a toggle keeps every CR after the header.
  Typed lines keep the unchanged lines' endings and take the body's own
  style.
- **BPR-1298:controls.** The core and Qt tests fail with the change removed.

## Implementation — 2026-10-06 (`bpr-main-linux`)

**Core.** `bpr::core::restore_line_endings(edited, original)`
(`src/core/{include/bpr/core,src}/line_endings.*`) is pure, with no Qt.

- An unedited text returns the original byte for byte, mixed endings
  included.
- Otherwise every line the edit left alone at the start or the end keeps its
  own ending.
- Every other break takes the original's dominant ending: CRLF when CRLF
  breaks are at least as many as lone LF ones, else LF.
- An original without breaks gives LF, so a new body is LF.

**Qt.** `review_draft_editor` keeps `LoadedBody`, the body last written into
it, and reads its text back through `body_text()`, which applies
`restore_line_endings`. That covers every read that leaves the editor: the
revisioned and legacy edit commands, the deferred-edit comparison, the
suggestion mark and the presentation comparison.

The in-place reply editor mirrors this editor: the window writes the reply
box's text into the draft editor, which publishes through `body_text()`. So
replies are covered by the same path. The two composition reads that pass the
unsent editor text to an agent as context (`github_application.cpp`, about
line 16388) still read LF. They only feed an agent and never write a body.

**Tests.**

- `core_line_endings`: unedited LF, CRLF, mixed, empty and break-ended bodies
  come back exactly. A header edit on a CRLF body with a suggestion fence and
  mixed tail keeps every later ending. Edited, inserted, removed and appended
  lines take the body's style. The dominance rule is checked both ways.
- `qt_review_draft_editor`, `test_a_crlf_body_keeps_its_line_endings`: a
  presented CRLF body reads back exactly and sends nothing. The label
  controls' typed edit (`issue`) and the blocking toggle keep every CR after
  the header. Typed text keeps unchanged endings and takes CRLF.
- The editor's member audit records `LoadedBody` as plain text.

**Failing-before controls.**

- With `body_text()` returning the editor's LF text, `qt_review_draft_editor`
  fails "a presented CRLF body reads back byte for byte, and presenting sends
  nothing".
- With `restore_line_endings` returning its input, `core_line_endings` fails
  "an unedited body returns exactly".

Both pass again restored.
