---
schema: "implement-rfc.card.v1"
id: "BPR-1730"
title: "Verify and close RFC-BPR-0090"
status: "Done"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-1726", "BPR-1727", "BPR-1728", "BPR-1729", "BPR-1733"]
closeout_for: ["RFC-BPR-0090"]
platforms: ["mac","win","linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1726--BPR-1733--4ee9bc97752e.json","origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2","milestone":"RFC-BPR-0090 closeout"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"685229cd0479cab4935cd181998fb298ad2f336e","evidence":{"kind":"command","command":"python3 scripts/verify_documentation.py --root . && python3 tests/scripts/verify_documentation_tests.py","detail":"Every RFC-BPR-0090 criterion, AC01 to AC10, is recorded in the closeout checklist with its check, result and producing commit, and passes: AC03 live on BPR-Test #123 with GitHub's Commit suggestion on an LF and a CRLF file, AC10 by the owner's look in attended session 7dba4a1f1dbf08a0ad93da313b35fbcd, AC08 on fixtures across a force push, its BPR-Test half (BPR-1733 AC1) waived by the owner on 2026-10-21 ('Accept the test-data evidence.'). Every dependency (BPR-1726 to BPR-1729, BPR-1733) is Done. The focused suites are reused from their recorded revisions, fc30dfbdb the latest; the source changes after it are comment ID substitutions (BPR-1734) and one start-review probe sentence (BPR-1715) that no RFC-BPR-0090 criterion uses. RFC-BPR-0090 is set to Auto-Verified and its index row reads 'Closed 2026-10-21 by BPR-1730'. The documentation gate and its self-tests pass on the closing commit.","refs":["docs/issues/60-done/BPR-1733-carry-an-in-pane-suggestion-to-a-newer-head.md","docs/issues/60-done/BPR-1729-a-live-course-and-the-owners-look-at-in-pane-suggestions.md","docs/issues/_checkpoints/RFC-BPR-0090-M4/handoff.md"]}}]
---

# BPR-1730: Verify and close RFC-BPR-0090

The closeout card for [RFC-BPR-0090](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md):
AC01 to AC10 with the evidence of milestones M1 to M4 and [[BPR-1733]].

## Acceptance criteria

- AC1: Every RFC-BPR-0090 criterion is recorded with its check, result and
  commit, and the RFC's status is set to match.

## Closeout checklist

RFC base: `3927fca2154f51fd85a9d32a181eb1f165479884`.

| Criterion | Direct check / evidence | Result at the reconciliation | Producing commit |
|---|---|---|---|
| **AC01:** From Shift+Return, the right-click item and the review bar, on a selection or the caret alone, the head pane becomes editable with unchanged text, scroll, numbers and colours, and the caret at the end of the last selected line. A base line, a range across changed regions, a single line outside every changed region and a range on a filtered diff whose right side is not head are refused at entry with their reason. | `qt_workbench_line_draft`, `qt_workbench_conversation_reveal` (test_compose_in_the_drawer, test_return_in_the_code_starts_a_draft_on_the_selection, test_right_click_on_any_line_and_on_base_lines, test_line_numbers_select_lines); BPR-1727 AC1 | Pass: every entry opens the head pane at its geometry with the caret at the last line's end; base line, a range across regions, a line outside every region and a range on a filtered diff are refused with their reason | `e249fc5f6` |
| **AC02:** Inside the range, RC55-07's gestures (multi-caret, box with virtual space, IME, undo, clipboard, Enter indentation) work in the pane on the composed application. An edit that would change a fixed byte is refused whole with the status line's words; Enter grows the range and the rows below move; emptying the span is a deletion. | Window key path courses (test_the_pane_edit_takes_the_editor_gestures and the fixed-line refusal); `core_pull_request_suggestion`; BPR-1727 AC2 | Pass: box, IME, undo, paste, Enter indentation in the pane; a fixed-byte edit refused whole with the status line's words; Enter grows the range; emptying is a deletion | `e249fc5f6`, `860b7726f` |
| **AC03:** For fixture edits that replace, delete, grow and shrink the lines and hold backticks, the published comment's `start_line`, `line`, `side` and body match the core's plan byte for byte, in the fixture service and on BPR-Test. GitHub's "Commit suggestion" then gives exactly the edited file, for an LF file and a CRLF file (the line-ending measurement). | `core_pull_request_suggestion` (the plan for replace, delete, grow, shrink, backticks); `application_line_draft_contract` (a three-line suggestion published through the fixture service with `start_line`, `line`, `side` and the core's composed body exact); the BPR-Test course and the owner's Commit suggestions (BPR-1729 AC1) | Pass: the five planned suggestions read back identical on BPR-Test #123; GitHub's one commit gave exactly the edited LF file and the edited CRLF file, CRLF on every line including the suggested one | `860b7726f`, `9cf0d41b2`, `dbc6c0f71`, `fc30dfbdb` |
| **AC04:** An unplaceable suggestion is marked with its reason on the comment and on the Finish your review page, cannot be published as a suggestion, and Convert gives the core's text body at the same lines, published where the core places a text comment. No suggestion is moved to another line. | test_a_held_suggestion_is_converted_and_a_kept_one_reopens; `qt_review_submission_editor_*`; BPR-1728 AC1 | Pass: held with its reason on the comment and on Finish your review, not sendable, Convert writes the core's text body at the same lines; nothing moves | `a7b8b0f6d` |
| **AC05:** The drawer shows the prose and "What it changes" and never a prefilled block. A label chip edit changes only the header's bytes of the composed body (`core_feedback_suggestion_preservation` extended to composed bodies). | test_compose_in_the_drawer (the field holds only the words); `core_feedback_suggestion_preservation` over composed bodies; BPR-1726 AC1, BPR-1728 AC1 | Pass: the drawer holds the prose and What it changes, never a block; a label edit changes only the header's bytes | `860b7726f`, `a7b8b0f6d` |
| **AC06:** A kept suggestion survives a file switch, closing the window and a killed process. Reopening rebuilds the same span exactly, and an unparsable block, a second block or another comparison is refused with its reason. | Reopening by Comment rebuilds the span; `core_pull_request_suggestion` rebuild refusals; the durable draft path; BPR-1726 AC2, BPR-1728 AC1 | Pass: the body goes through the durable draft path; reopening rebuilds the same span; unparsable, second block and other comparison refused | `860b7726f`, `a7b8b0f6d` |
| **AC07:** Undo, Delete, leaving the edit and typing back to the original behave as "Lifecycle" says. | Window courses for Undo, Esc, typed back and Delete; BPR-1728 AC1 | Pass: as Lifecycle says | `a7b8b0f6d` |
| **AC08:** With a new head arriving mid-edit, the edit ends and the draft stays on its comparison. Carry is offered exactly when every original line of its range is unchanged at the new head, and Copy change otherwise. | diff_workbench::adopt_publication ends the edit (BPR-1728 AC1); `core_pull_request_suggestion`, `application_epoch_host`, `qt_workbench_window_*` and `application_commit_filter_public_path` (a_suggestion_carries_across_a_force_push); BPR-1733 AC1 | Pass on fixtures: Carry exactly when every line is unchanged and together, Copy change otherwise, on the composed application across a force push. The RFC criterion names no live run; BPR-1733's own AC1 also named BPR-Test, which the live harness cannot drive (its composer writes no draft), and the owner waived that half on 2026-10-21 ("Accept the test-data evidence.") | `860b7726f`, `a7b8b0f6d`, `fc30dfbdb` |
| **AC09:** On a filtered diff whose right side is not head, a line unchanged at head is placeable, and a changed one is unplaceable with its reason. | test_a_held_suggestion_is_converted_and_a_kept_one_reopens; `core_pull_request_suggestion` filtered reasons; BPR-1726 AC3, BPR-1728 AC1 | Pass: unchanged at head is placeable; changed is held with its reason | `860b7726f`, `a7b8b0f6d` |
| **AC10:** The owner accepts the built in-pane edit on a frozen build, one question per look criterion: entry and continuity, the fixed lines' refusal, growth and deletion, the drawer link, the unplaceable mark and Convert, and one pane. | Attended session `7dba4a1f1dbf08a0ad93da313b35fbcd` on the frozen build of `b13164f02` (`a7b8b0f6d` here); BPR-1729 AC2 | Pass: the owner accepted all six questions; the two notes became the read-only refinement (`d2db849ab`) and BPR-1734 | `ddc878a47` |

## Reconciliation, 2026-10-22

Every RFC-BPR-0090 criterion passes at the reconciliation, AC08 on fixtures.
The RFC is not closed yet: BPR-1733, a dependency, keeps its own AC1 open on
its BPR-Test half, which the live harness cannot drive (its review composer
writes no draft). Once the coordinator decides that half (an owner wizard on
a frozen build, or reading the card's criterion as the RFC's), BPR-1733
closes, this card records its AC1 and the RFC's status is set to Done.

## Final closeout record, 2026-10-21

- **BPR-1733.** The owner accepted the test-data evidence on 2026-10-21
  (chat, relayed by the coordinator): AC1 passes on the fixtures, which is
  the RFC's AC08 with no live run, and its BPR-Test half is waived. The card
  is Done, so every dependency of this card is Done.
- **Diff since the RFC base.** From `3927fca21` the source and test changes
  are `ad9dbb5f1` and `683147056` ([[BPR-1734]], the owner's note on the
  one-pane look, Done), `d2db849ab` (the read-only refinement from AC10's
  other note), `fc30dfbdb` ([[BPR-1733]]) and `87a3fff13` ([[BPR-1715]]'s
  follow-up to a start-review probe string, outside this RFC). Every RFC
  criterion is accounted for in the checklist.
- **Checks at closeout.** The documentation gate
  (`python3 scripts/verify_documentation.py --root .`) and its self-tests
  (`python3 tests/scripts/verify_documentation_tests.py`) pass on the closing
  commit. The focused suites behind AC01 to AC09 are reused from their
  recorded revisions, `fc30dfbdb` the latest: the source changes after it
  replace `BPR-PENDING-AJ` with `BPR-1734` in comments and change one
  expected status sentence in the start-review course, which no
  RFC-BPR-0090 criterion uses, so nothing those suites run has changed. No
  build was made for the closeout.
- **Follow-ups.** [[BPR-1731]] (a stacked two-pane comparison, Q4's note)
  and [[BPR-1732]] (a converted suggestion can always become a file comment)
  stay in the backlog. Windows and Linux are unmeasured; AC10's look was on
  macOS.
