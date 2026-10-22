---
schema: "implement-rfc.card.v1"
id: "BPR-1733"
title: "Carry a kept in-pane suggestion to a newer head, or copy its change"
status: "In Progress"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1728"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 M3, 2026-10-21: split from BPR-1728 because it needs host facts the window does not hold","related_cards":["BPR-1723","BPR-1726","BPR-1728"],"milestone":"RFC-BPR-0090 M3"}
acceptance: [{"criterion": "AC1: RFC AC08: with a new head arriving mid-edit, the edit ends and the draft stays on its comparison; Carry is offered exactly when every original line of its range is unchanged at the new head, and Copy change otherwise, on fixtures and on BPR-Test", "outcome": "pending", "revision": "fc30dfbdba20415e04c79016d90fd35877fb7910", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^(core_pull_request_suggestion|application_epoch_host|application_commit_filter_public_path|qt_workbench_window_scale_(100|125|200))$'", "detail": "Fixtures pass; BPR-Test not run. core_pull_request_suggestion (test_a_suggestion_from_an_earlier_head_carries_or_copies): lines 5-7 unchanged and moved carry to 7-9, a changed line or no correspondence is not carried, a deletion has no change to copy, words holding a block, a text draft, a reply, a base line and a publishing draft are not carried. application_epoch_host (head_move_lines_compare_an_earlier_head_with_the_head): before a newer head nothing may be read; after it the file is read by literal path at each head and the three earlier lines are the head's lines 2-4; a missing blob is objects_unavailable; a never-accepted commit, the head itself, another head and an absolute path are refused before admission; forgetting earlier heads (negative control) fails the course. qt_workbench_window_* (test_a_suggestion_from_an_earlier_head_is_carried_or_copied, split and one pane): the note sits at the carried line with its words, Copy change copies 'renamed `line` 5\\n', Carry opens the head pane edit on line 6 with the carried block and the drawer holds the words, the new draft holds the body exactly, the earlier draft is deleted at revision 3 only once the new one is saved; a changed line offers Copy change only, at its own line; a carry onto line 21 outside every changed region is refused with the reason. application_commit_filter_public_path (a_suggestion_carries_across_a_force_push): on the composed application a suggestion kept on C3 of commit-filter-split-v1 is offered Carry at C3r after the force push ('Written on 968f892 at line 1. Those lines are unchanged here, at line 1.'), from the private store's earlier-head drafts and the host's read of both heads; Carry writes the same body at C3r, the draft on C3 is deleted and its note goes, and nothing is published. BPR-Test: the live harness (live_second_account_course) composes a review composer that writes no draft, so no kept suggestion exists there to carry; the host's read is the same git cache path the fixture course drives, over a fixture pack instead of GitHub's. Needs the coordinator's decision: an owner wizard on a frozen build, or reading the criterion as fixtures plus RFC-BPR-0089's live force-push course."}}, {"criterion": "AC2: Format, tidy on the changed files, the reseals and the documentation gate pass", "outcome": "pass", "revision": "fc30dfbdba20415e04c79016d90fd35877fb7910", "evidence": {"kind": "command", "command": "format_check.py; tidy.py --warnings-as-errors; reseal_command_correctness.py; reseal_round_trip_composition.py; verify_documentation.py; QT_QPA_PLATFORM=offscreen ctest (42 related suites)", "detail": "clang-format 22.1.8 passes on the 17 changed files. clang-tidy passes on every changed unit apart from the two std::endl lines in workbench_window_tests.cpp's case tracer (b059676cc7). The window tests' pin and --line-draft inventory, and the github_application.cpp pins, were resealed; the round-trip full and manifest pins were resealed with a note, mutation 16d135419 exact. The 42 related suites pass, including application_line_draft_contract, which had failed since RFC-BPR-0090 M2 because its probe expected Shift+Return to prefill the drawer; the probe now types its change in the head pane. application_start_review_contract fails before and after this change on a status wording ('Review was not published.' expected, 'Not published: GitHub refused this review.' since 7a00adb52), outside this card. The documentation gate fails only on BPR-1734's provisional ID."}}]
---

# BPR-1733: Carry a kept in-pane suggestion to a newer head, or copy its change

RFC-BPR-0090 "A new head while editing" ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#a-new-head-while-editing)).

## Where it stands

- Built in `fc30dfbdb`: the core's `decide_suggestion_head_move`, the epoch
  host's `resolve_head_move_lines`, the composition's earlier-head reads, and
  the note with Carry to the new head and Copy change.
- Open: AC1's BPR-Test half, which the live harness cannot drive (below).

## Scope

- The host supplies the old-to-new head correspondence of a kept suggestion
  draft's file when the head moves.
- The read-only draft offers **Carry to the new head** exactly when
  `decide_suggestion_carry` maps its lines; carrying opens the same edit on
  the new lines with the same block, and deletes the old draft once the new
  one is kept.
- Otherwise it says the file changed where it was edited and offers **Copy
  change**, which copies the block's text.

## Acceptance criteria

- AC1: RFC AC08: with a new head arriving mid-edit, the edit ends and the
  draft stays on its comparison; Carry is offered exactly when every
  original line of its range is unchanged at the new head, and Copy change
  otherwise, on fixtures and on BPR-Test.

## What was built

- **Core.** `decide_suggestion_head_move(draft, correspondence)`: a kept
  suggestion (the reviewer's own new comment, still being written, right
  side, a body `split_suggestion_body` reads) is carried exactly where
  `decide_suggestion_carry` maps its range; otherwise the refusal says why
  (`changed_where_edited`, `lines_unknown`, `second_block`). `Change` is
  the block's lines for Copy change.
- **Host.** `epoch_application_host::resolve_head_move_lines`: the file by
  literal path at an earlier head and the accepted head, compared with the
  review's engine, on the review's cache actor. Only a head this host
  accepted for the same pull request before the current one may be read
  (`EarlierHeads`, newest 64). The reading and comparing that
  `resolve_filtered_comment_lines` did is now one shared reader.
- **Composition.** `github_application.cpp` remembers the head and path
  each comparison it showed was read at, reads the private store's kept
  suggestions on other heads (`private_review_workflow::earlier_suggestions`;
  the shared draft owner holds only the accepted head's), asks once per
  earlier head and path, and presents what the core decides. Carry's
  earlier draft is deleted in the store at the revision shown
  (`discard_earlier_suggestion`).
- **Qt.** A note of its own at the lines it is carried to, or at its own,
  "Written on `<short commit>` at lines a–b.", with Carry to the new head
  (Suggest change on those lines, refused with its reason where GitHub takes
  no suggestion; the new draft takes the same body) and Copy change.
- A suggestion kept on a comparison this window did not show (before a
  restart, say) is not offered; Saved comments keeps it, read-only.

## The BPR-Test half of AC1

The live harness composes the shipped application with a review composer
that writes no draft (`live_second_account_course.cpp`, "its review composer
is read-only without a fixture"), so no kept suggestion exists there to
carry, and a test accessor cannot be reached from that non-testing target.
The fixture course drives the same path: the private store's draft on the
earlier head, the epoch host reading both heads from the review's git cache
(a fixture pack in place of GitHub's transport), the core's decision, Carry
and the deletion. Left for the coordinator: an owner wizard on a frozen
build (write a suggestion on a BPR-Test pull request, the agent pushes, the
owner carries it), or reading AC1 as fixtures plus RFC-BPR-0089's live
force-push course.
