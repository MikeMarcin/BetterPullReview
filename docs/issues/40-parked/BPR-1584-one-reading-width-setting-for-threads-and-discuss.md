---
schema: "implement-rfc.card.v1"
id: "BPR-1584"
title: "One reading width for Threads and Discuss, as a setting, never for the code"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner notes 2026-10-17, relayed by the coordinator","owner_note":"is it weird that threads is seemingly fixed-width ... I don't necessarily hate it for ultra-wide monitors ... Discuss should probably have the same behaviour. We can make that capped width a setting.","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1290","BPR-1293","BPR-1512","BPR-1556","BPR-1581","BPR-1582"],"reservation":"docs/issues/_reservations/BPR-1584--BPR-1584--5ffb2022a4ca.json"}
parked_on: "human-review"
---

# BPR-1584: One reading width for Threads and Discuss

## Owner notes

Relayed by the coordinator on 2026-10-17, from the owner's screenshot of
Threads in a wide window: the rows stop at about 70% of the width, the words
end in "…" beside a large empty area, and the people and counts float in the
middle of the window. "is it weird that threads is seemingly fixed-width".
Then: "I don't necessarily hate it for ultra-wide monitors", "Discuss should
probably have the same behaviour. We can make that capped width a setting."
And a hard rule: the cap never applies to Review's comparison panes; two
whole files side by side with the drawers open on an ultra-wide monitor is a
key feature.

## Why Threads is 880 pixels wide

`conversation_finder.cpp` caps every row at `list_maximum = 880`, labelled
"the prototype's measures (`docs/design/BPR-1290-inline-drawer-v2`)". The
prototype's stylesheet has `.flist-in { max-width:880px }` for the finder in
the drawer-sized frame and `.fwin .flist-in { max-width:none }` for the
finder that fills the window. [[BPR-1293]] built the window-filling one and
kept the drawer-sized measure. No card or RFC chose a reading measure for
Threads; the cap is a porting slip.

Capped surfaces found: Threads' rows (880) and search field (520), the
Comments drawer (280–760, a drawer's own bounds), Discuss's list beside the
composer (340), and Finish review's single column. Discuss's notes and
Threads' rows are the reading lists this setting governs. The search field,
the drawer and the composer's list keep their own bounds. The code
comparison is never capped.

## Design

Designed with the `frontend-design` skill. One rule for both lists: the
column starts at the page's left edge, as the search and Discuss's band do,
and stops at the reading width. The people, counts and state stay at the
row's right end, and the words take every pixel before them. Settings →
Reviews & queue gains **Reading width**: **Narrow** (960 px), **Medium**
(1600 px), **Wide** (2560 px, the default), **Full width**. Its info line
reads "Threads and Discuss stop at this width on a wide screen. The code
comparison always uses the whole window." The change applies at once to every
open window, and the section's reset returns it to Wide.

The core holds the vocabulary and the rule (the column's width for a page
width); the profile's workbench settings store it (schema 22, existing rows
read as Wide), and Qt applies it.

## Acceptance

- `BPR-1584:core`: the core's rule gives each choice's column for narrow,
  ordinary and ultra-wide pages, and Full width never caps.
- `BPR-1584:storage`: `sqlite_contract` stores and reloads every choice,
  migrates a version 21 file to Wide, and refuses an unknown value.
- `BPR-1584:surfaces`: at 1280, 1900, 2560 and 3440 pixels Threads' rows and
  Discuss's notes span the page up to the setting's width, with the trailing
  people and counts at the row's right end; a live change resizes both.
- `BPR-1584:code`: at every choice, at 3440 pixels with the drawers open, the
  comparison panes fill the width the drawers leave.
- `BPR-1584:negative`: a negative control (a cap left in place, a
  comparison pane narrowed) is reported.
- `BPR-1584:gates`: format, tidy, documentation gate, the shared full `dev`
  ctest.
- `BPR-1584:owner-look` (human): the owner accepts renders of both surfaces
  at a normal and an ultra-wide width for each choice, in the same short
  session as [[BPR-1581]] and [[BPR-1582]], one question.

## Checkpoint: candidate v001, 2026-10-17

One milestone with [[BPR-1581]], [[BPR-1582]] and [[BPR-1584]]: one shared
core route for every move between Threads, Review and Discuss, and one branch.
Product candidate `c6644bad7e3ecc05a70965bcd9910ef2a173725c` on branch
`bpr-1581-thread-navigation` (worktree
`.claude/worktrees/bpr-1581-thread-navigation`), main merged in at
`b6b066285`. Not merged to main until the owner accepts. Paths, commands and
identities are in [the checkpoint](../_checkpoints/BPR-1581/handoff.json).

Verification at the candidate (macOS, `dev` Debug, offscreen):
`core_workbench_interaction`, `core_review_ledger`,
`application_review_ledger_host`, `sqlite_workbench_settings_storage`,
`qt_conversation_finder_scale_{100,125,200}`, `qt_conversation_cards_view`,
`qt_thread_moves_scale_{100,125,200}` (new), `qt_keyboard_shortcuts_scale_100`,
`qt_workbench_conversation_reveal`, `qt_workbench_window_scale_100` and
`application_profile_multi_window_public_path` (the composed Mark as Unread
course) pass. Negative controls: an unknown thread moves nothing; the keys
step aside while a reply is typed; a single click picks nothing; Full width
leaves Discuss uncapped and a 1900-pixel window's rows run past the old 880;
the read command is disabled with no read path; Mark read undoes Mark unread.
Fails before: the core and window courses do not compile without
`route_workbench_thread`'s origin, `choose_workbench_thread_target`,
`mark_conversation_unread` and `reading_column_width`; the finder's click
course asserts that a press picks nothing, where the unchanged sources pick on
a press (not rerun against the old build). clang-format 22
clean; clang-tidy 22 reports nothing in the touched sources; documentation gate
`DOCUMENTATION_OK`. The round-trip composition contract's pins already differ
on main (`06b00593…` against the pinned `315f7078…`) without this branch; the
reseal covering `src/app`'s new Mark as Unread wiring is done at landing on the
merged tree. The full `dev` suite did not complete: the shared volume ran out
of space during the full build (828/859), so it remains owed at landing; see
the checkpoint.

Owner look: session `c1a9372af55350e3cb63fe7d8377eafa`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1581-v001`,
repo root the main checkout, ready at event cursor 9, one Accept/Reject
question per card; frozen root
`/Users/jaeger/bpr1581-thread-moves-v001.x7yze1z4` (executable SHA-256
`e5ba8757f9d1…`, its `--check` reached `ATTENDED_FILE_SHOWN`), launch
`python3 /Users/jaeger/bpr1581-thread-moves-v001.x7yze1z4/launch.py`.

This card's candidate: `BPR-1584-reading-width-v001 (fingerprint `cd4c2a71db92…`)`, criterion `BPR-1584:owner-look`, Pending.
