---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-AS"
title: "A long thread in the Comments drawer scrolls to its Reply field and past its end, and a refused reply says why"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"kind":"bug","origin":"owner bug report, 2026-10-22, relayed by the coordinator; owner amendment the same day","introduced_by":"201c96f21 (BPR-1290, 2026-10-06) and 3d1f5941e (BPR-1581, 2026-10-17) for the wheel; cc0387486 (RFC-BPR-0037, 2026-09-19) and 5484b3fb5 (BPR-1383, 2026-10-10) for the notice","related_cards":["BPR-0657","BPR-1046","BPR-1133","BPR-1290","BPR-1291","BPR-1383","BPR-1581","BPR-1698"]}
acceptance: [{"criterion": "AC1: A long thread opened in Review from Discuss scrolls by wheel, Page Down, End and Home until its Reply field is in view, and focus reaching the Reply field brings it into view; with a current thread hanging from its line and with an outdated one opened among its file's unplaced threads; the course fails without the fix", "outcome": "pass", "revision": "b3a6e12efb74eb7665b5994011d9957c29f3acc2", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^qt_workbench_conversation_reveal$'", "detail": "test_a_long_thread_from_discuss_scrolls_to_its_reply opens a four-post thread about three drawer heights tall from Discuss (line button for the current course, Open in Review for the outdated one), and drives the window with QWindowSystemInterface clicks and wheel steps and delivered keys. Before the fix the outdated course's 30 wheel steps all went to the 400-line code and the thread stayed at the drawer's top; Home did nothing. Run alone it also passes at QT_SCALE_FACTOR 2.0 and in the built-in dark theme."}}, {"criterion": "AC2: No false notice after Discuss's Open in Review; a refused reply says what is true of the thread and offers Show in code when the thread's file is not the one in the code; the course fails without the fix", "outcome": "pass", "revision": "b3a6e12efb74eb7665b5994011d9957c29f3acc2", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^qt_workbench_conversation_reveal$'", "detail": "Before the fix the Discuss refusal read 'Open this thread in code before replying.' and stayed after Open in Review opened the thread beside its line, in both courses. With the fix the notice goes once the thread opens; Show in code opens the thread in Review in one click; an outdated thread's Reply says its line isn't in the code shown, offers no button, and the notice does not cover the focused Reply field."}}, {"criterion": "AC3 (owner amendment, 2026-10-22): the Comments drawer scrolls past its end until only its last line is at the top, as the code panes do; End goes there and Home back; Discuss's feed is unchanged", "outcome": "pass", "revision": "b3a6e12efb74eb7665b5994011d9957c29f3acc2", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R '^qt_conversation_cards_view$|^qt_workbench_conversation_reveal$'", "detail": "test_the_drawer_scrolls_past_its_end: two short notes in a 600 px drawer scroll until the last note's last line is at the top (maximum is the content's foot less two line heights); End and Home reach both ends; a feed shorter than its view does not scroll; an unplaced open thread takes the wheel before a code that takes every step. The window course checks the long thread's foot ends within three lines of the drawer's top."}}, {"criterion": "AC4: The conversation-reveal, cards-view, line-draft and composed Qt suites, format, tidy on the changed files, the reseal and the documentation gate pass", "outcome": "pass", "revision": "b3a6e12efb74eb7665b5994011d9957c29f3acc2", "evidence": {"kind": "command", "command": "QT_QPA_PLATFORM=offscreen ctest -R 'qt_conversation_cards_view|qt_workbench_conversation_reveal|qt_workbench_line_draft|qt_github_conversation_panel|qt_composed_|qt_conversation_finder|qt_thread_scroll_stability|composed_markdown|qt_workbench_window_scale'; format.py; tidy.py --warnings-as-errors; reseal_command_correctness.py; reseal_round_trip_composition.py --check; verify_documentation.py", "detail": "The 32 related suites pass (rerun on the rebased candidate: the 13 conversation, line-draft and composed drawer, long-thread and github suites). clang-format 22.1.8 check passes on the 8 changed C++ files. clang-tidy with warnings as errors passes on conversation_cards_view.cpp, github_conversation_panel.cpp, workbench_window.cpp and both test files. The window tests' pin and the --conversation-reveal inventory were resealed and command_correctness_registry_tests.py passes; the round-trip pins are unchanged (no src/app change). The documentation gate fails only on this card's provisional ID; verify_documentation_tests.py passes 67/67."}}]
---

# BPR-PENDING-AS: A long thread in the Comments drawer scrolls to its Reply field and past its end, and a refused reply says why

## Owner report

2026-10-22, verbatim: "I have a comment with a long reply in the review side
drawer and I'm not able to scroll on the review drawer at all to see the the
reply field. It might just barely be hidden behind 'open this thread in code
before replying' which I also have no idea what that means I'm on the
code... I did open in review from discuss"

Owner amendment, the same day, verbatim: "Much like the code window I want
to be able to scroll the comments drawer up until only the last line is
visible."

## Root causes

1. **The wheel went to the code, and the thread did not move with it.** In
   Review the drawer moves with the code ([[BPR-1290]], `201c96f21`): a wheel
   over the notes scrolls the code first, and the notes' own overflow takes
   only what the code cannot. That is right for a thread that hangs from its
   line. An outdated thread, or one whose line the comparison does not hold,
   opens among its file's unplaced threads ([[BPR-1581]], `3d1f5941e`) and
   does not move with the code. Every wheel step scrolled the code under it,
   and the thread only moved once the code reached its end. Reproduced
   offscreen: 30 steps on a 400-line file, thread top fixed at 4 px.
2. **The drawer's end was its Reply field's foot.** The scroll range ended
   with the last note's foot at the drawer's foot. A refusal notice under the
   drawer then took two lines of its height, and the focused Reply field went
   under the notice. Nothing kept it in view when the drawer shrank.
3. **A stale notice that was untrue and gave an instruction.** "Open this
   thread in code before replying." (`cc0387486`) is shown when Reply cannot
   select the thread's line. In Discuss that happens whenever the thread's
   file is not the one in the code. The notice is cleared only by a pull
   request presentation, so it stayed after Discuss's Open in Review
   ([[BPR-1383]], `5484b3fb5`) had opened the thread beside its line. For an
   outdated thread the condition is real, but the notice told the reviewer to
   open what was open. Replying needs a draft on the thread's current line
   (`draft_on_thread_line`), which an outdated thread no longer has.
4. **Focus did not reach a Reply field below the code's end.** Revealing a
   focused control moved only the code. Near the end of a short file the code
   could not move that far, and nothing took the rest.

## Fix

- The cards view gives an open thread that does not hang from a line the
  wheel itself first, and the code only what the notes cannot take.
- The drawer scrolls past its end as the code panes do ([[BPR-1291]]): its
  furthest position leaves the last note's last line, and the inset under it,
  at the top. Discuss's feed keeps its end. Home and End, and the platform's
  document start and end, go to the notes' top and end, as in the code panes.
- Revealing a focused control takes from the notes what the code could not
  give. When the drawer shrinks, its focused control stays in view.
- The window says why a reply was refused, in words true of the thread:
  - "This thread is on long.cpp, which isn't the file in the code.", with a
    Show in code chip that opens the thread in Review;
  - "This thread's line isn't in the code shown, so it can't take a reply
    here.";
  - "This thread is on the pull request's base, and this comparison starts
    from another commit.";
  - "This thread is on the pull request's latest commit, and this comparison
    ends at another one."

  The refusal goes once it may no longer be true: when a reply starts, when a
  pick opens its thread, when Show in code is used, and when the modality
  changes. The thread menu's disabled Reply gives the same reason as the
  second notice.
- The notice row in `github_conversation_panel` can carry one action, drawn
  as the panel's own chip so both themes follow the palette.

Designed with the `frontend-design:frontend-design` skill: plain sentences
that name the file, and a button named for what it does.

## Acceptance criteria

- AC1: A long thread opened in Review from Discuss scrolls by wheel, Page
  Down, End and Home until its Reply field is in view, and focus reaching the
  Reply field brings it into view. This holds for a current thread hanging
  from its line and for an outdated one opened among its file's unplaced
  threads. The course fails without the fix.
- AC2: There is no false notice after Discuss's Open in Review. A refused
  reply says what is true of the thread, and offers Show in code when the
  thread's file is not the one in the code. The course fails without the fix.
- AC3 (owner amendment, 2026-10-22): The Comments drawer scrolls past its end
  until only its last line is at the top, as the code panes do. End goes
  there and Home goes back. Discuss's feed is unchanged.
- AC4: The conversation-reveal, cards-view, line-draft and composed Qt
  suites, format, tidy on the changed files, the reseal and the documentation
  gate pass.

## Not in scope

Replying to an outdated thread. GitHub accepts a reply to one, but this
application's reply is a draft on the thread's current line. Offering it
needs a reply that has no line, which is a product decision for the owner.

## Result

Landed in `b3a6e12ef` on the implementer's branch. The provisional ID is the coordinator's
to allocate.
