---
schema: "implement-rfc.card.v1"
id: "BPR-2312"
title: "Notes check: private history keeps its store; the legacy author label and thread migration go"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"cleanup","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2310--BPR-2313--b73895df6230.json","origin":"owner, 2026-11-20: \"All, incl. notes check\" (vestigial items cut; no legacy support for old data)","related_cards":["BPR-2310","BPR-2311"],"acceptance":["AC1","AC2","AC3"]}
---

# BPR-2312: Notes check: private history keeps its store; the legacy author label and thread migration go

## Asked

Owner, 2026-11-20: "All, incl. notes check". RFC-BPR-0127 ("Comments absorb
notes") replaced private notes with Local comments, yet
`private_review_panel.cpp`, `private_composition_editor.cpp` and
`private_review.cpp` remain, with a `legacy_retained_subject` migration and a
"Legacy · author unknown" label. The check: do Local comments use this code;
cut what is unreachable or superseded, keep what is depended on.

## Findings

The private-review code is not the notes feature. It is RFC-BPR-0023's
private thread store, and it is live:

- `private_review_panel` is the Agent tab's History page
  ([[BPR-1594]], RFC-BPR-0085 D5): `workbench_window_central_surface.cpp`
  builds it and hands it to `agent_tab_view` as its history widget, and the
  review submission editor's "saved comments" reveal opens it.
- `private_composition_editor` is opened by `private_review_workflow`
  (composition `PrivateReview`) to compose private turns and drafts.
- `core/private_review.cpp` and the SQLite private review store hold the
  threads, revisions, agent runs, retained drafts and publications, and the
  `private_local_comment` table that **is** Local comments' storage
  (`private_store_local_comment`, `private_load_local_comments`).

So nothing here is unreachable except one function. What RFC-BPR-0127
retired (`local_note_record` calls, the Notes tab) was already gone.

## What changed

- `core::migrate_private_thread` is removed: nothing outside its own test
  called it (the SQLite migration builds note threads directly).
- The private history's author label: a revision whose actor is
  `private_legacy_unknown_actor` (a note moved in from before private
  threads, which only the reviewer could have written) reads "Reviewer";
  the "Legacy · author unknown" text is gone.

## Kept, and why

- `legacy_retained_subject` and `private_legacy_unknown_actor`. They are
  alternatives of `private_thread_binding` and `private_review_actor`, which
  the store encodes by alternative index: removing them would renumber every
  stored binding and actor. Rows with a NULL binding are threads the
  schema 11 to 12 move kept because an agent run refers to them.
- The ledger's note migrations (`migrate_private_review`,
  `migrate_private_note_paths`, `migrate_private_note_lines`,
  `migrate_private_local_comments`) and the `restore_schema_11_notes` test
  hook. `review_ledger_database.cpp` creates every new ledger at schema 1 and
  runs these steps up to 12, so they are how a new profile gets its private
  review and Local comment tables. Removing them needs a direct schema-12
  creation and validation; that is a schema change of its own, not a cut.

## Acceptance

- AC1. The check above is recorded with each kept piece and its reason.
- AC2. `migrate_private_thread` and the "Legacy · author unknown" text are
  gone; a legacy-actor revision renders as "Reviewer".
- AC3. Build, focused tests, `architecture_boundaries`, format and tidy on
  changed files, and the documentation gate pass.

## Evidence

Dev preset, macOS, worktree branch `claude/vestigial-cuts` on main
`c937eab1b`:

| Criterion | Check | Result |
|---|---|---|
| AC2 | `core_private_review` (a legacy binding still never borrows the current revision; new text cannot claim the legacy actor), `core_review_cycle_feedback` | Passed |
| AC2 | `qt_private_review_panel` (the legacy actor renders "Reviewer", no "Legacy") | Compiles; not run, because the Qt adapter build did not fit the shared disk |
| AC3 | `architecture_boundaries`; `format.py`, `format_check.py` (clang-format 22.1.8) and `tidy.py --warnings-as-errors` on every changed C++ file; `verify_documentation.py` | Passed |
