---
schema: "implement-rfc.card.v1"
id: "BPR-1339"
title: "An accepted file shows as accepted on Overview, and counts as done"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
parked_on: "human-review"
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-08","reservation":"docs/issues/_reservations/BPR-1339--BPR-1339--66d5f024f602.json","related_cards":["BPR-1331","BPR-0647","BPR-0656","BPR-0972","BPR-1211"],"acceptance":["BPR-1339:reproduced","BPR-1339:rule","BPR-1339:overview-mark","BPR-1339:counts","BPR-1339:owner-look"]}
---

# BPR-1339: An accepted file shows as accepted on Overview, and counts as done

## Owner notes

Owner, chat, 2026-10-08, verbatim: "I accepted a file in the review but it
still shows as unread in the overview".

The rule, the owner's answer the same day, verbatim: "Rule gap, marking it read
updates it to read. Accepted is stronger than read or unread".

## What exists

- The receipt (`core::file_receipt`, RFC-BPR-0007) holds three independent
  facts: the read progression (unread, seen, read), accepted, and excluded.
  Accepting a file does not mark it read, and with the default "mark read by
  hand" behaviour opening a file does not either.
- [[BPR-1331]]'s `core::progress_mark` already sums a receipt up as one mark:
  excluded, then accepted, then the read progression. Only the review bar's
  private progress button uses it.
- Overview ([[BPR-0647]], RFC-BPR-0028's "my read state") projects only the read
  fact: `overview_file_row::Read`, the read column's dot, "Files read N of M",
  the heading's "N not read", and Continue review's "first file I have not
  read". Acceptance never reaches the row. `overview_receipt_index::apply`
  ([[BPR-0972]]) also drops a receipt change whose read state did not move, so
  an Accept published no row update at all.
- The Files drawer ([[BPR-0656]]) draws its faint unread ring from the same
  read fact, so an accepted unread file kept that ring too.

The refresh path itself works: marking a file read in Review moves its
Overview row at once. The fault is the rule gap, not a stale projection.

## Scope

- **Rule.** Accepted is stronger than read, seen and unread. A file's row shows
  the core's `progress_mark`, so an accepted file shows accepted whatever its
  read state. Acceptance still does not set the read fact: revoking it returns
  the row to the file's read state.
- **Counts.** A file is done when it is read or accepted
  (`core::progress_done`). "Files read N of M", the heading's "N not read" and
  Continue review's next file all count done files. Exclusion's counting is
  unchanged (by its read state).
- **Overview mark.** The read column draws the mark in the BPR-1331 family:
  hollow unread, half seen, filled read as before; accepted a filled disc with
  a tick cut out; excluded a ring struck through. Its accessible text and
  tooltip say "accepted" or "excluded".
- **Files drawer.** The faint unread ring shows only while the mark is unread.
- **Live.** An Accept, a Revoke acceptance, an Exclude and an Include move the
  row, the counts and the next file through the same incremental update a read
  change uses.

## Acceptance

- `BPR-1339:reproduced`: a composed course on the nested-folders fixture
  accepts one file through the private progress control's menu and another
  through its key and mnemonic, then shows Overview; against the code before
  the change it fails with the accepted file still "unread".
- `BPR-1339:rule`: the owner's rule is recorded verbatim here and in
  RFC-BPR-0007; `core_review_ledger` pins `progress_done`.
- `BPR-1339:overview-mark`: application and Qt tests pin the row's mark, the
  incremental update on accept and revoke, and the read column's text; each
  fails against the code before the change.
- `BPR-1339:counts`: the summary, the heading and Continue review count an
  accepted file as done, in the projection and in the incremental update.
- `BPR-1339:owner-look`: the owner accepts the Overview's accepted mark
  (`overview-accepted-mark-v001`) in one short attended session on a frozen
  build.

## Reproduction

`application_accepted_file_in_overview` (new, `tests/end_to_end/`) on the
nested-folders fixture: opens `bpr-fixtures/nested-folders#21`, enters Review,
accepts `README.md` by pointer in the private progress menu (✓▾) and
`docs/api/reference.md` by ⌥⇧4, Down and Return, marks
`docs/api/v2/endpoints.md` read, and shows Overview. Against the code before
the change (base `1ed20ce1f`) both accepted rows said "unread", the read one
"read", Files read "1 of 17" and the heading "16 not read". Both accept paths
worked in Review; only Overview ignored acceptance. The default "mark read by
hand" behaviour means opening a file does not mark it read, so an accepted file
stayed at unread or seen.

Cause: (b), a rule gap, not a stale projection. `overview_file_row` carried
only the read fact, so neither the complete projection nor the incremental
`overview_receipt_index::apply` saw acceptance; the latter also skipped any
receipt change whose read state did not move, so an Accept published no row.
Marking read moved the row at once, as the owner said.

## Outcome

- `core::progress_done` (file_progress_mark.hpp): read or accepted.
- `overview_file_row::Mark` holds `core::progress_mark`; the projection and
  the incremental index set it, count done files in `Progress.FilesRead`, keep
  the not-done rows for Continue review, and compare read state and mark.
- Overview's read column: accessible text and tooltip say the mark
  ("accepted", "excluded"); the delegate draws accepted as the read dot's
  colour, one unit wider, with BPR-1331's tick cut out, and excluded as a ring
  struck through. `overview_file_model::progress_role` carries the mark.
- Files drawer: `files_tree_model::progress_role`; the faint unread ring shows
  only while the mark is unread.
- Spec: the owner's rule recorded in RFC-BPR-0007 "Receipt semantics" and the
  RFC-BPR-0028 Overview paragraph.

`frontend-design:frontend-design` was loaded before the mark design.

## Verification

macOS arm64, `build/dev` (debug info off for disk), offscreen, candidate
`e431a9087` (rebased on `2cbab3dd0`; it landed as `2573a8acf` on `55dec39a4`,
whose new commits touch none of this card's files):

- `ctest -R '^(core_review_ledger|application_pull_request_overview|qt_pull_request_overview_page|qt_files_tree|application_accepted_file_in_overview|qt_review_state_icon|qt_workbench_window_scale_(100|125|200)|application_active_changed_scaling|application_overview_summary_(1|2)|architecture_boundaries)$'`:
  all pass. `qt_pull_request_overview_page` also passes at `QT_SCALE_FACTOR=2`.
- Failing-before controls: the course fails at "Overview says accepted for
  each accepted file" on `1ed20ce1f`. Dropping acceptance from
  `progress_done` fails `core_review_ledger` ("an accepted file is done though
  only seen"); comparing only the read state in `apply` fails
  `application_pull_request_overview` ("accepting a seen file moves its row
  though its read state did not move"); not drawing the accepted mark fails
  `qt_pull_request_overview_page` ("each mark draws differently: none and
  accepted").
- clang-format clean on every changed file. A direct clang-tidy run on the
  four changed product files and the course reports nothing on changed lines.
- Offscreen renders looked at: the five marks at 2x, and the composed
  Overview after the course (accepted, accepted, read; "3 of 17", "14 not
  read").

Windows and Linux have not run this change; it adds no platform code.

## Acceptance status

- `BPR-1339:reproduced`: pass at `e431a9087`.
- `BPR-1339:rule`: pass at `e431a9087`.
- `BPR-1339:overview-mark`: pass at `e431a9087`.
- `BPR-1339:counts`: pass at `e431a9087`.
- `BPR-1339:owner-look`: pending.

## Owner look, session v001

Pending: `BPR-1339:owner-look` is the owner's. One short session, one
candidate, one question, Accept or Reject.

- Session `1f11263fa5bc958f20f92909f9d7e841`, its own fresh state
  `/Users/jaeger/.mask/attended-review/owner-review-20261008-bpr1339-overview-accepted-v001`,
  ready and protected at event cursor 3. Candidate
  `BPR-1339-overview-accepted-mark-v001` ("Overview's accepted mark",
  revision `frozen-2573a8acf-v001`), with the 2x render of the five marks. The
  host was started detached and answered on loopback.
- Frozen root `/Users/jaeger/bpr1339-overview-accepted-v001.3IHAyM`:
  `git archive` of `2573a8acf` into `src/`, configured with `app-release` and
  `BUILD_TESTING=ON` into `src/build/app-release-tests`, built
  `better_pull_review`, source read-only; executable SHA-256
  `6abb9eb9c18145054bea05d03386c43b64b0bc9d0a35fe100816ea8037e8bd95`.
  Offscreen with the launcher's arguments it printed
  `ATTENDED_FILE_SHOWN:src/engine/renderer.cpp`.
- Launch: `python3 /Users/jaeger/bpr1339-overview-accepted-v001.3IHAyM/launch.py`
  (`conventional-comments-v1`, signed in, `src/engine/renderer.cpp` open in
  Review). It checks the build and the fixture tree first and reports an exit
  within seconds.

Next: read the session's decisions (`status --after 3`). Accept satisfies
`:owner-look` and the card finishes; a Reject comes back with its note for a
v002.
