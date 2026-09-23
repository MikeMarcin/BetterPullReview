---
schema: "implement-rfc.card.v1"
id: "BPR-1029"
title: "Windows: the Discuss course keeps a thread-card iterator across its own reply press, so qt_workbench_window_scale_* and qt_workbench_conversation_reveal abort under MSVC debug iterators"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win","found_at":"919e2154","rechecked_at":"7bf7de5b","related_cards":["BPR-0649","BPR-0653","BPR-0985"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1029--BPR-1031--9a8ec6194169.json"}
acceptance: [{"criterion":"AC1 qt_workbench_conversation_reveal and qt_workbench_window_scale_{100,125,200} pass on the Windows station","outcome":"pass","revision":"8edd961a","evidence":{"kind":"command","detail":"bpr-main-win (Windows 11 10.0.26200, MSVC 19.51 Debug, Qt 6.11.2), serial, offscreen, at 2b14139f with the BPR-1029/1030/1031 changes (measured on 7d68d5a3's source with the three cards' changes before they were rebased and pushed; BPR-1029 landed as 8edd961a, and the Qt adapter and adapters_qt sources these rows build are identical there and in the measured tree): qt_workbench_conversation_reveal passes 16.60 s (was: abort 0xc0000409, can't dereference invalidated vector iterator, 15.76 s at 7bf7de5b); qt_workbench_window_scale_100 passes 104.31 s, _125 110.22 s, _200 105.09 s alone. In one serial batch _200 failed once at 'sized publication completes its native measurement before scroll evidence', a timing assertion in another course, recorded on the card.","path":"docs/issues/60-done/BPR-1029-windows-the-discuss-course-keeps-a-thread-card-iterator-across-its-own-reply-press.md#verification"}},{"criterion":"AC2 A cards-view course fails against the reference-handing dispatch and passes with the copy, in both the action and the thread dispatch","outcome":"pass","revision":"8edd961a","evidence":{"kind":"command","detail":"qt_conversation_cards_view with test_a_dispatch_that_relays_out_still_names_the_pressed_card: against the original view it fails 'an action whose dispatch relays the cards out still names the pressed thread'; with only the action copy it fails 'a press whose dispatch makes another thread current still names the pressed thread'; with both copies it passes (14.88 s). A reverted probe showed the view's layout counter go from 13 to 14 inside the Reply dispatch.","path":"docs/issues/60-done/BPR-1029-windows-the-discuss-course-keeps-a-thread-card-iterator-across-its-own-reply-press.md#fixed-in-the-course-and-in-the-view-bpr-main-win-2026-09-23"}},{"criterion":"AC3 The other rows that compose the conversation panel keep their prior Windows result","outcome":"pass","revision":"8edd961a","evidence":{"kind":"command","detail":"qt_github_conversation_model_property passes 169.34 s; qt_public_preview_conversation passes 1.01 s; qt_composed_github_scale_{100,125,200} fail at BPR-0869's 'the code is in a fixed-pitch font at 1.6 times the row's size', as before; qt_github_conversation_panel fails at 'narrow Discuss begins in the focused conversation index', the same line with the view rebuilt from 7bf7de5b's source, so it predates this change (ID requested).","path":"docs/issues/60-done/BPR-1029-windows-the-discuss-course-keeps-a-thread-card-iterator-across-its-own-reply-press.md#verification"}}]
---

# BPR-1029: Windows: the Discuss course keeps a thread-card iterator across its own reply press, so qt_workbench_window_scale_* and qt_workbench_conversation_reveal abort under MSVC debug iterators

## Defect

On the Windows station (MSVC Debug, whose checked iterators are on),
`qt_workbench_conversation_reveal` and `qt_workbench_window_scale_100`, `_125`
and `_200` abort with exit `0xc0000409`:

```
...\MSVC\14.51.36231\include\vector(67) : Assertion failed: can't dereference invalidated vector iterator
```

Rechecked at `7bf7de5b`: `qt_workbench_conversation_reveal` aborts after
15.76 s with exactly that line.

All four rows run
`test_discuss_uses_main_content_and_open_in_code_returns_to_read` in
`tests/adapters_qt/workbench_window_tests.cpp`. The course takes `sameFile`
and `offFile`, which are iterators into the feed's
`conversation_cards_view::cards()`. It presses the off-file card's Reply, and
then dereferences `sameFile->Actions` to find Open in code. The press rebuilds
that vector, because `conversation_cards_view::relayout()` clears it and
refills it, so the iterator is spent. MSVC's debug iterators abort. Unchecked
iterators on libstdc++ and libc++ read whichever card was rebuilt in the same
slot. Here that is the same card, which is why macOS and Linux pass.

**All four rows are offscreen.** They take `bpr_qt_github_common_environment`
(`QT_QPA_PLATFORM=offscreen`, `tests/adapters_qt/CMakeLists.txt`), run inside
the AppContainer, and show no window. They need no foreground clearance.

## Scope

- In the course, find the same-file card again after the Reply press.
- The same shape exists in the product. `conversation_cards_view::mousePressEvent`
  hands `ActionDispatch` a reference to `Cards[index].Id`, and `choose()` hands
  `ThreadDispatch` a reference to `Current`. The window's dispatch can rebuild
  the vector, or make another thread current, before it reads the identity it
  was given. Hand each dispatch its own copy.

## Acceptance

- AC1 `qt_workbench_conversation_reveal` and
  `qt_workbench_window_scale_{100,125,200}` pass on the Windows station.
- AC2 A cards-view course fails against the reference-handing dispatch and
  passes with the copy, in both the action and the thread dispatch.
- AC3 The other rows that compose the conversation panel keep their prior
  Windows result.

## Fixed in the course and in the view (bpr-main-win, 2026-09-23)

Run `bpr-main-win-20260923`. Windows 11 Home 10.0.26200, MSVC 19.51 Debug,
Qt 6.11.2, `build/dev`. Logs are under `build/logs/bpr-1029-main-win/` and
`build/logs/bpr-1029-1031-final-main-win/` on the station and are not
committed.

**What rebuilds the vector.** A reverted probe read the view's layout counter
around the course's Reply press: 13 before the press, 14 when `sendEvent`
returned, and 14 after `process_events`. So the feed is laid out again inside
the window's Reply dispatch, before that dispatch reads its identity and
before any event is processed. The course's iterators are spent, and so is the
reference the view handed the dispatch.

**The course.** `test_discuss_uses_main_content_and_open_in_code_returns_to_read`
finds the same-file card again after the Reply press, and expects it to
survive the refused reply.

**The view.** `conversation_cards_view::mousePressEvent` copies
`Cards[index].Id` before calling `ActionDispatch`, and `choose()` copies
`Current` before calling `ThreadDispatch`.

**The course that pins it.**
`test_a_dispatch_that_relays_out_still_names_the_pressed_card`
(`tests/adapters_qt/conversation_cards_view_tests.cpp`) presses a card whose
action dispatch adopts the conversations in the opposite file order before it
reads the identity. It then presses a card whose thread dispatch makes the
other thread current first. Both dispatches must report the pressed thread.
Negative controls, one at a time:

- With the view's original action dispatch: `FAILED: an action whose dispatch
  relays the cards out still names the pressed thread`. The reference named
  the card rebuilt in slot 0.
- With the copy in the action dispatch only: `FAILED: a press whose dispatch
  makes another thread current still names the pressed thread`.

### Verification

Measured on `7d68d5a3`'s source with the BPR-1029, BPR-1030 and BPR-1031
changes applied, before they were rebased and pushed. They landed as
`8edd961a`, `8d6a01bf` and `574a93cf` on top of `2c83b65c`; the Qt adapter
and `tests/adapters_qt` sources these rows build are identical in both.
Serial, offscreen:

| Row | Before | After |
|---|---|---|
| `qt_workbench_conversation_reveal` | Aborts, `can't dereference invalidated vector iterator`, 15.76 s at `7bf7de5b` | Passes, 16.60 s |
| `qt_workbench_window_scale_100` | Aborts in the same course | Passes, 104.31 s |
| `qt_workbench_window_scale_125` | Aborts in the same course | Passes, 110.22 s |
| `qt_workbench_window_scale_200` | Aborts in the same course | Passes, 105.09 s alone. In the serial batch it failed once at `sized publication completes its native measurement before scroll evidence`, a timing assertion in a different course, which passed at 108.47 s before the rebase |
| `qt_conversation_cards_view` | Passes | Passes, 14.88 s, with the new course |

The other rows that compose the conversation panel keep their prior Windows
result:

- `qt_github_conversation_model_property` passes (169.34 s).
- `qt_public_preview_conversation` passes (1.01 s).
- `qt_composed_github_scale_{100,125,200}` fail at `the code is in a
  fixed-pitch font at 1.6 times the row's size`, which is [[BPR-0869]]'s
  measured Windows failure.
- `qt_github_conversation_panel` fails at `narrow Discuss begins in the
  focused conversation index`, before any dispatch. The same line fails with
  the view rebuilt from `7bf7de5b`'s source, so the failure is older than
  this change. It has no record, and an ID has been requested for it.

`clang-format --dry-run --Werror` (22.1.8) is clean on the touched files.
