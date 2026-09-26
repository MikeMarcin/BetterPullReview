---
schema: "implement-rfc.card.v1"
id: "BPR-1133"
title: "Comment and Reply where the line and the thread are"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-08","AC-10"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 Comment opens the inline page on the current line's draft, or on a reply draft for the selected thread.","outcome":"pass","revision":"7c99a1eb","evidence":{"kind":"command","detail":"macOS 26.6.2 arm64, build/dev Debug: qt_workbench_window_scale_{100,125,200} pass test_comment_names_its_target_and_every_card_replies: with line 10 selected Comment opens composeInlineDraftPage and names no thread; on line 22 Comment names tbutler's thread through the review reply dispatch and opens the inline page; its text, tooltip, Pull Request menu entry and arrow menu name the target ('Comment on line 10…', 'Reply to tbutler's thread…'); the arrow's menu offers the plain comment alone. Control A (Comment ignoring the thread) fails. The owner's attended look is pending on BPR-1135."}},{"criterion":"AC2 A card beside the code replies to its own thread.","outcome":"pass","revision":"7c99a1eb","evidence":{"kind":"command","detail":"qt_conversation_cards_view passes with Reply on the cards beside the code and comment_here removed; the window course, with jiayincaonv's thread's line selected, presses Reply on tbutler's visible card beside the code: the comparison selects line 22 and the reply names review-thread:t22 on the inline page. Controls B (no card fallback for the line) and C (no Reply beside the code) fail."}},{"criterion":"AC3 Nothing is published without the existing explicit Publish.","outcome":"pass","revision":"7c99a1eb","evidence":{"kind":"command","detail":"The window course records every review draft command and asserts none is begin_publish after Comment on a line, Comment on a thread and a card's Reply; application_threaded_reply_contract and application_review_submission_composition_public_path pass. Comment and Reply only open the page; Publish remains the page's."}}]
---

# BPR-1133: Comment and Reply where the line and the thread are

## Outcome

Comment, the review bar's icon and Option-C (Alt+C), starts a comment on the
current line or a reply to the selected thread. Its menu names the target:
"Comment on line 431…" or "Reply to tbutler's thread…". Its arrow is the
place RFC-BPR-0048's labels will be offered. Until then it offers the plain
comment only, and no label is invented here.

Thread cards beside the code offer Reply, as the accepted Read asset draws
them. The reply goes through the path the Discuss feed's Reply already uses.
They stop offering Comment here, which starts a second thread on the same
line.

## Acceptance

- Comment opens the inline page on the current line's draft, or on a reply
  draft for the selected thread.
- A card beside the code replies to its own thread.
- Nothing is published without the existing explicit Publish.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-08, AC-10 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.

## Execution record

What was built, how it was verified, and what is owed elsewhere. The
acceptance above is unchanged by it.

## Done on macOS, 2026-09-25 (main, `7c99a1eb`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order (see
the RFC's status section), so this slice started without waiting for
RFC-BPR-0028 to RFC-BPR-0038.

### What changed

- **Comment's target.** Comment (`composeInlineDraftAction`, the review bar's
  icon, Option-C, and the Pull Request menu) answers the selected review
  thread when there is one and otherwise starts a comment on the current line.
  The selected thread is the one the reviewer chose (a card, the list, the
  map) or, outside Discuss, the thread on the line they selected; a thread the
  conversation list made current only by default answers nothing, and in
  Discuss the hidden comparison's line is never a target. The command's text
  and tooltip name the target wherever it is offered: "Comment on line 431…"
  and "Reply to tbutler's thread…" ("Comment (⌥C)" with neither). It follows
  the comparison's selection and the conversation's current thread.
- **Comment's arrow.** The bar's Comment button is a menu button: the icon
  runs Comment, and its arrow opens `commentMenu`, which offers the
  target-named plain comment alone. RFC-BPR-0048's labels go there; none is
  invented here, and no key is added.
- **One Reply path.** Comment's reply and every card's Reply go through
  `reply_to_thread`: the thread becomes current, its line is selected, which
  composes the draft there, and only then is the thread named
  ([[BPR-0657]], [[BPR-1046]]), waiting for the thread's comparison if it is
  still loading. Either way the inline composition page opens; nothing is
  published without its Publish.
- **Every card replies.** Thread cards beside the code offer **Reply**, as the
  Discuss feed's do; `card_action::comment_here` ("Comment here", which started
  a second thread on the line) is gone. When another line is selected the
  conversation list holds only that line's threads, so a card's Reply takes
  its thread's line from the card (`select_thread_line` falls back to it); the
  old "Comment here" failed there too.
- **Specification.** [workbench-specification.md](../../ux/workbench-specification.md)
  describes Comment and Reply under "Primary layout", and the key table names
  Comment's reply.

### Verification (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug, at `7c99a1eb`)

| Check | Result |
| --- | --- |
| `qt_workbench_window_scale_{100,125,200}` | Pass. `test_comment_names_its_target_and_every_card_replies` (new): the arrow's menu holds the plain comment alone; no target before a comparison; the comparison's first change (line 13), then line 10, named in the text, the tooltip and the Pull Request menu; Comment on a line opens the inline page and names no thread; on line 22 it names and answers tbutler's thread (reply dispatched, inline page), and Esc returns; with line 5 selected (jiayincaonv's thread) the tbutler card, visible beside the code, offers only Reply and thread-state actions, and its Reply selects line 22 and answers that thread; no publish command is ever sent |
| `qt_conversation_cards_view` | Pass: cards beside the code offer Reply (and Mark read, Resolve); leaving the feed restores Reply without Open in code; a press reports Reply for the card's own thread |
| `qt_github_conversation_panel`, `qt_workbench_focus_order_scale_{100,125,200}` (no key used twice), `qt_workbench_title_row_scale_{100,125,200}`, `qt_composed_github_scale_{100,125,200}`, `qt_workbench_conversation_reveal`, `application_threaded_reply_contract`, `application_review_submission_composition_public_path`, `application_profile_multi_window_public_path`, `application_agent_{finding,invocation}_composition_contract`, `application_workbench_composition_contract`, `application_review_round_trip_composition_contract`, `test_network_isolation_manifest`, `architecture_boundaries`, `core_dependency_boundary` | 27 of 27 pass |
| Control A, scratch: Comment ignores the selected thread | Fails: `Comment (Option-C) names the selected thread and opens the inline page on the reply draft` |
| Control B, scratch: no card fallback for the thread's line | Fails: `the card's Reply selects its own thread's line and answers that thread on the inline page` |
| Control C, scratch: cards beside the code offer no Reply | Fails: `the card is as tall as its posts and offers Reply to its own thread ([[BPR-1133]])` |
| clang-format 23.1.1 | Clean on the six touched C++ files (their `HEAD` versions were clean) |
| `_coding-standard/scripts/tidy.py --changed` | `clang-tidy passed`; its findings are on lines this card did not touch |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The controls were scratch edits, restored and rebuilt. The renders are at
`build/dev/tests/adapters_qt/workbench-window-{100,125,200}-comment-reply-cards.png`
and `…-comment-menu.png` (not committed).

### Owed elsewhere: the Windows and Linux runs

The change is portable Qt, but the menu button and the card actions are drawn
in each platform's style. Each station runs, at or after `7c99a1eb`:

```sh
ctest --test-dir <build> --output-on-failure -R "^(qt_workbench_window_scale_(100|125|200)|qt_conversation_cards_view|qt_github_conversation_panel|qt_workbench_focus_order_scale_100|qt_composed_github_scale_100|application_threaded_reply_contract|application_review_submission_composition_public_path|test_network_isolation_manifest)$"
```

- **Windows (bpr-main-win): owed.** Expect ten passes, and the
  `workbench-window-*-comment-*.png` renders in the build's `tests/adapters_qt`.
- **Linux (bpr-main-linux): owed.** Expect ten passes.

A failure on either is this card's defect.

### Owner asks (for the attended look BPR-1135 needs; no session prepared)

Each ask is one question about one criterion.

1. **Comment starts a comment on the current line, or a reply to the selected
   thread.** On a line that has a thread, Comment (⌥C) now replies to that
   thread, and its tooltip and menu say "Reply to tbutler's thread…"; on a line
   without one it says "Comment on line 431…". Is replying the right default on
   a thread's line?
2. **Its menu names the target, and its arrow is where the labels will go.**
   Until RFC-BPR-0048, Comment's arrow opens a menu holding only the
   target-named plain comment. Is that arrow and menu what you want in the
   meantime?
3. **A card beside the code replies to its own thread.** The cards beside the
   code now offer Reply where they offered "Comment here". Is that the card you
   chose?

## Owner verdicts — 2026-09-25 (attended session 06ad5734, macOS)

Packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md),
session `06ad5734ed9f7c2fd6d0a62c8328fa0a`, hand-back at sequence 54. The
questions were carried verbatim, with identical fingerprints, from session
`60bd1ed5`, which still lists them unanswered. They bind the frozen m2 app:
source `ba9d2723`, executable `a2e5128a…8a83`. Every decision is `actor:
human`, and its fingerprint equals the prepared one. This card's acceptance
above is unchanged; the work the verdicts ask for is on new cards.

| Ask | Candidate, decision | Verdict, verbatim | Disposition |
|---|---|---|---|
| 1. Comment on a thread's line | `BPR-1133-default-reply`, `1a095bfe`, seq 51 | "Reply is the right default" | Accepted on macOS |
| 2. The arrow and its menu | `BPR-1133-arrow`, `b3d1d32d`, seq 49 | "Keep the current arrow and menu", with the note "note: clicking comment takes me to this screen in the screenshot which seems completely broken, Doesn't seem like I can actually leave a comment yet." | Arrow and menu accepted on macOS; the page Comment opened is [[BPR-1207]] |
| 3. Reply beside the code | `BPR-1133-card-reply`, `37c42e44`, seq 50 | "Keep Reply on these cards" | Accepted on macOS |
