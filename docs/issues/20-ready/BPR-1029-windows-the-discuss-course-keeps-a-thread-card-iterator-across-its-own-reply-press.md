---
schema: "implement-rfc.card.v1"
id: "BPR-1029"
title: "Windows: the Discuss course keeps a thread-card iterator across its own reply press, so qt_workbench_window_scale_* and qt_workbench_conversation_reveal abort under MSVC debug iterators"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win","found_at":"919e2154","rechecked_at":"7bf7de5b","related_cards":["BPR-0649","BPR-0653","BPR-0985"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1029--BPR-1031--9a8ec6194169.json"}
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
