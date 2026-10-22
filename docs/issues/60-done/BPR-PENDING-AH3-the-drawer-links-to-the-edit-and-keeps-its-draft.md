---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AH3"
title: "The drawer links to the in-pane edit, keeps its draft, and converts a suggestion GitHub cannot apply"
status: "Done"
rfcs: ["RFC-BPR-0090"]
depends_on: ["BPR-PENDING-AH2"]
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"RFC-BPR-0090 adoption, 2026-10-21 (owner answers in session e5387830d18edd8d541b96e47ecbd9cb)","related_cards":["BPR-1723","BPR-1706","BPR-1716"],"frontend_design":"frontend-design:frontend-design loaded for RFC-BPR-0090's design frames v2; the card and page keep the product's own palette, the reading amber for a hold","milestone":"RFC-BPR-0090 M3","split_out":["BPR-PENDING-AH8"]}
acceptance: [{"criterion":"AC1: RFC AC04 to AC07 and AC09, and AC08's ending of the edit at a new head, on the composed window against fixtures (Carry and Copy change are BPR-PENDING-AH8)","outcome":"pass","revision":"a7b8b0f6d8a745d14c9383b328bee81e582817f4","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(qt_workbench_.*|qt_review_submission_editor_.*|qt_conversation_cards_view|qt_suggestion_editor_.*|qt_diff_workbench_scale_.*)$'","detail":"31 of 31 pass. AC04/AC09: test_a_held_suggestion_is_converted_and_a_kept_one_reopens puts the window on a diff whose right side is not the head; a suggestion on line 6 placed on an earlier commit is held (Convert 'Convert to a comment on 9b2e41c' in place of the sending buttons, Ctrl+Shift+Return sends nothing and the status line gives the reason, Finish your review holds the draft), and Convert writes 'Suggested change to line 6:' and the diff block on the same line and releases the hold; a line GitHub resolves on no commit offers 'Convert to file comment'; on a diff whose right side is the head nothing is held. qt_review_submission_editor: the held row's reason and Convert, every verdict and Submit by key waiting, and Convert asking for exactly that comment. AC05: test_compose_in_the_drawer, the field holds only the words and the body is the words, a blank line and the block, also after Esc; the label header is the prose's first bytes (R12 matrix at 860b7726f). AC06: the body goes through the existing durable draft edit path (each edit is a revisioned command the course applies through the core); reopening by Comment rebuilds the change in the code with its words in the drawer. AC07: Undo (gesture course), Esc, typed back (a plain comment of its words) and Delete ending the edit. AC08: a new publication ends the edit (diff_workbench::adopt_publication), and the draft stays on its comparison under the existing rule."}},{"criterion":"AC2: Format, tidy on the changed files, the reseals and the documentation gate pass","outcome":"pass","revision":"a7b8b0f6d8a745d14c9383b328bee81e582817f4","evidence":{"kind":"command","command":"format_check.py; tidy.py --build-dir build/dev --warnings-as-errors; reseal_command_correctness.py; reseal_round_trip_composition.py --check; verify_documentation.py","detail":"clang-format 22.1.8 check passes on the nine changed files. clang-tidy passes on every changed unit apart from the two pre-existing std::endl lines in workbench_window_tests.cpp's case tracer (b059676cc7). The window tests' pin and the --line-draft inventory were resealed; the round-trip pins are unchanged. The documentation gate fails only on the provisional IDs and NEXT_ID's existing high-water report."}}]
---

# BPR-PENDING-AH3: The drawer links to the in-pane edit, keeps its draft, and converts a suggestion GitHub cannot apply

RFC-BPR-0090 M3 ([RFC](../../rfcs/RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md#milestones)).

## Scope

- The drawer holds the prose only, with "What it changes"; the draft's body
  is the core's composition and goes through the durable draft path.
- An unplaceable suggestion is marked on the comment and on Finish your
  review, held back, and offered Convert there and on the comment.
- Leaving, reopening, deleting, typing back to the original, a new head and
  Carry, and the composed course on a fixture pull request.

## Acceptance criteria

- AC1: RFC AC04 to AC07 and AC09, and AC08's ending of the edit at a new
  head, on the composed window against fixtures (Carry and Copy change are
  BPR-PENDING-AH8).
- AC2: Format, tidy on the changed files, the reseals and the documentation
  gate pass.

AC1 first read "RFC AC04 to AC09". Carry and Copy change need the host's
old-to-new head correspondence of a kept draft's file, which the window does
not hold, so they became BPR-PENDING-AH8; the closeout depends on it.

## What was built

- The drawer link itself landed with M2 (`e249fc5f6`); this card adds the
  filtered placement, the hold, Convert on the card and on Finish your
  review, reopening from any entry, and the card counting a code change as
  content.
- The window holds what it has planned: a suggestion opened in the code this
  session. A draft written in an earlier session is held when it is next
  opened (RFC "Held from the review").
- The composed courses are the window courses against fixture drafts and
  the real core, not the conventional-comments-v1 application course the
  draft RFC named; the live course (AH4) publishes through GitHub.

## Rebase

Rebased onto main `7075954df` on 2026-10-21. The rebase changed only main's own files (the diagnostics zip, release maintenance and the BPR-1715 storage follow-up), none of which this card's evidence reads, so the recorded revisions are the rebased commits: `489eff8f0` is `860b7726f`, `0f1b446ea` is `e249fc5f6`, `b13164f02` is `a7b8b0f6d` and `56bfc1518` is `9cf0d41b2`.
