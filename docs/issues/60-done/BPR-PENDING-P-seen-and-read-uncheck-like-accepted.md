---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-P"
title: "Seen and Read uncheck from the review bar, as Accepted does"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner request, 2026-10-21: In the toolbar I can uncheck accepted by clicking the accepted button. I should be able to do the same for read and seen.","governing_rfc":"RFC-BPR-0007","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1331","BPR-1339","BPR-1506","BPR-1507","BPR-1515","BPR-1578"],"frontend_design_skill":"loaded before the step behaviour, labels and tooltips were designed"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "ae0ea34f848c4bcb206ea4b8e5efd33b69416598", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^core_review_ledger$'", "detail": "review-ledger-core-tests:pass. test_read_and_seen_can_each_be_taken_off: unmark_read takes a read file to seen (marked seen, not done), is legal and changes nothing on a seen or unread file, keeps an accepted file accepted, shown accepted and done, keeps an excluded file excluded, and is refused against replaced material; mark_unread takes a read or seen file to unread; on an accepted file both keep it accepted and done, and revoking afterwards shows the progression the reader left. Failing-before control: the test does not compile without the core command (no member unmark_read). macOS 26 arm64, Qt 6.12 offscreen, dev preset Debug built -O0 -g0 to fit the disk, on the branch rebased onto main 0b10fa71a."}}, {"criterion": "AC2", "outcome": "pass", "revision": "ae0ea34f848c4bcb206ea4b8e5efd33b69416598", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^application_review_ledger_host$'", "detail": "review-ledger-host-tests:pass. The projection offers mark_unread in Seen's slot once seen and unmark_read in Read's once read. test_seen_and_read_toggle_and_round_trip_through_the_store walks each slot's projected command through the host: read and accepted, then Read off (seen, still accepted), then Seen off (unread, still accepted), then Seen and Read on and off again; after each, a new host over the same store reads exactly that receipt back. Failing-before control: with the core command and the old projection the host test fails at 'the four compact slots carry the inverse gestures and core legality'."}}, {"criterion": "AC3", "outcome": "pass", "revision": "ae0ea34f848c4bcb206ea4b8e5efd33b69416598", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(qt_review_state_icon|qt_workbench_window_scale_(100|125|200)|qt_option_keys_typing|qt_workbench_focus_order_scale_100|qt_workbench_title_row_scale_100)$'", "detail": "All pass. test_progress_track_shows_and_takes_each_step: the steps are checkable and their accessible state says checked; a seen file's Seen step is solid, checked and enabled; Space on the checked Read step dispatches unmark_read and a click on the checked Seen step mark_unread, each against the exact material, the step staying as the saved receipt says until it arrives; the menu reads Mark unread and Mark u&nseen and dispatches the same commands; the published receipt moves the track, the checked states and the progress button (seen, then unread); an accepted file's Read step takes Read off and the file stays accepted. test_mark_read_when_fully_seen: Read taken off is not marked read again at the end of the same material. test_receipt_actions_follow_decided_projection: a seen file's Seen action is enabled as Mark u&nseen. Failing-before control: with the core command but the old projection and window, qt_review_state_icon fails at 'a seen file's Seen step is solid, checked and offers Mark unseen' and qt_workbench_window_scale_100 at 'accepted receipt projection decides checked state, inverse gesture, and status'."}}, {"criterion": "AC4", "outcome": "pass", "revision": "ae0ea34f848c4bcb206ea4b8e5efd33b69416598", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^application_accepted_file_in_overview$'", "detail": "ACCEPTED_OVERVIEW_PUBLIC_PATH_OK through the shipped composition, ledger host and SQLite store on nested-folders#21. The first file's checked Read step leaves it seen and its checked Seen step leaves it unread, which is retained while the file stays open; reopening records Seen. The revoked second file's Read step checks it read and Overview counts 3 done; unchecking leaves it seen, Overview says seen and counts 2. The accepted first file's Seen step unchecks it, and it stays accepted in the progress button, Overview and the Files drawer and still counts as done. Failing-before control: with the core command only, the course fails at 'the read file's Read and Seen steps are checked and clickable'."}}, {"criterion": "AC5", "outcome": "pass", "revision": "ae0ea34f848c4bcb206ea4b8e5efd33b69416598", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(application_profile_multi_window_public_path|application_active_changed_scaling|application_review_ledger_composition_contract|application_review_round_trip_composition_contract|application_uiforetw_selected_diff_contract|command_correctness_registry_contract|architecture_boundaries)$'; python3 _coding-standard/scripts/format.py --dry-run <the 12 changed C++ files>; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <the 10 changed translation units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py", "detail": "All pass. The multi-window and scaling courses now expect Mark unread to leave a file seen, and use Mark unseen for unread. The ledger composition probe requires an automatically seen file's Seen action to be checked. The round-trip contract's full (894053c121e8) and manifest (0e773b9c7ea8) pins are resealed for that one src/app line, and the mutation pin 16d135419d39 is unchanged. The command-correctness registry and workbench case inventory follow the two changed sources. clang-format 22.1.8 passes on all 12 changed C++ files. clang-tidy 22 --warnings-as-errors passes on every changed translation unit except two performance-avoid-endl findings in workbench_window_tests.cpp's runCase trace, which main already has and this change does not touch. Both documentation gates pass under a stand-in allocated ID; with the provisional ID the gate refuses only the card's id format, as it did for BPR-PENDING-M. The workbench specification, docs/architecture/review-ledger.md and RFC-BPR-0007's receipt semantics record the behaviour. macOS 26 arm64, Qt 6.12 offscreen, dev preset Debug built -O0 -g0 to fit the disk, on the branch rebased onto main 0b10fa71a."}}]
---

# BPR-PENDING-P: Seen and Read uncheck from the review bar, as Accepted does

## Owner note

The owner, 2026-10-21, verbatim:

> In the toolbar I can uncheck accepted by clicking the accepted button. I
> should be able to do the same for read and seen.

## What governed it

The receipt semantics are [RFC-BPR-0007](../../rfcs/RFC-BPR-0007-local-review-ledger-actionability.md)'s.
They never made Seen or Read one-way: `mark_unread` already existed and the
private-progress menu's Read item already toggled to Mark unread. The one-way
rule came from [[BPR-1578]]'s progress track, recorded in the
[workbench specification](../../ux/workbench-specification.md): "a step
already reached does nothing, so the track never un-reads a file (Mark unread
stays in the menu)". The owner accepted that track (v002), and this request
supersedes that one rule. The specification records the replacement, and
RFC-BPR-0007's receipt semantics carry a dated amendment for the new command.

## Behaviour

Read implies Seen, and acceptance is a separate fact (RFC-BPR-0007, amended
for [[BPR-1339]]: accepting does not mark a file read, and revoking
acceptance leaves the read progression where it was). The unchecks follow
from that:

- **Read**, checked, click: Read comes off and the file stays seen
  (`core::file_receipt_command_kind::unmark_read`, new). The menu's Read item
  keeps its name, **Mark unread**, and now does the same.
- **Seen**, checked, click: the file becomes unseen, which clears Read too
  (`mark_unread`, the existing command). The menu's Seen item reads **Mark
  unseen** on a seen file, as Accept reads Revoke acceptance.
- Neither uncheck touches acceptance or exclusion. An accepted file stays
  accepted, shows accepted and counts as done (`core::progress_done`); revoke
  it to see its read state again.
- Seen comes back by itself. Opening a file marks it seen ([[BPR-1506]]), once
  per presentation. A file marked unseen stays unseen while it is shown, and
  the next open marks it seen again (another file and back, a tab, or a return
  from Overview or Discuss). With "when fully seen", a window marks a file
  read at the end once for its material, so Read taken off is not put back.
- The Seen and Read steps are checkable buttons. Their checked state is the
  saved receipt's, so a screen reader hears "checked", and a click waits for
  persistence, as Accept does. Their tooltips and accessible descriptions say
  what a click does. An excluded file still rests the track.
- No key or menu item is added: no key unmarks Accepted (Option-A never
  revokes), and the menu's existing Seen and Read items carry the inverses.

The application's `project_file_receipt_actions` decides the slot's command
(Seen: `mark_seen` or `mark_unread`; Read: `mark_read` or `unmark_read`).
The core's `apply` decides the receipt, and the window dispatches the
projected command of the step's slot and makes none up. No schema change.

## Design

`frontend-design:frontend-design` was loaded first. The track's look is
unchanged. The owner chose it over two rounds of designs, so the change is in
behaviour, words and state. The two inverses are named for the file's fact,
Mark unseen and Mark unread, so the menu reads as a pair with Seen and Read,
and existing references to "Mark unread" on a read file stay true. Considered
and left out: a hover wash on the solid (reached) cell. It would change the
look of the accepted Accepted cell, which already unchecks without one. It is
a candidate for a later look.

## Acceptance

- AC1: The core takes Read off a read file to seen and Seen off to unread,
  keeps acceptance and exclusion, refuses replaced material, and does nothing
  to a file not read.
- AC2: The projection offers each inverse once reached, and the store holds
  what each toggle left for a later host to read back.
- AC3: In the window, clicking or pressing Space on each checked step
  dispatches its inverse. The steps, the menu items, the progress button and
  the track show the saved state, and Read taken off holds under "when fully
  seen".
- AC4: Through the shipped composition, the steps uncheck durably, unseen is
  retained while the file stays open, and Overview's marks and "Files read"
  counts follow. An accepted file unchecked from Seen stays accepted and done.
- AC5: The changed end-to-end courses, contracts and gates pass.

## Checkpoint

Product commit `ae0ea34f8` on branch `worktree-agent-a56ce533a10122405`, rebased
onto main `0b10fa71a`; this card follows in its own commit. Not landed on
main: the coordinator allocates the ID, renames `BPR-PENDING-P` in the
changed sources, tests and documents, and reseals the round-trip contract for
that rename.

Changed paths: `src/core/include/bpr/core/review_ledger.hpp`,
`src/core/src/review_ledger.cpp`,
`src/application/src/review_ledger_application_host.cpp`,
`src/adapters/qt/include/bpr/qt/workbench_window.hpp`,
`src/adapters/qt/src/workbench_window.cpp`, `src/app/src/github_application.cpp`
(one probe condition), the core, host, Qt and three end-to-end tests, the
round-trip contract, the command-correctness registry, the workbench case
inventory, `docs/ux/workbench-specification.md`,
`docs/architecture/review-ledger.md` and RFC-BPR-0007.

Failing-before, in stages on the same tree: the new tests did not compile
without `unmark_read`. With the core command alone, the core test passed and
the host test, `qt_review_state_icon`, `qt_workbench_window_scale_100` and
`application_accepted_file_in_overview` failed at the expectations named in
the acceptance evidence.

Observed and left alone: on nested-folders#21, with the third file read, the
Files drawer listed 16 files and not `docs/api/v2/endpoints.md`, and Enter on
that file's Overview row left Review on the file already open. The course
opens a file Enter does reach instead. Another card should look at it.

No attended review is pending. The change keeps the track's accepted look and
only adds behaviour, so no look-and-judge question is parked on the owner. The
build tree was removed at the end.
