---
schema: "implement-rfc.card.v1"
id: "BPR-1046"
title: "application_threaded_reply_contract fails at HEAD: since cc038748 a card's Reply selects the thread's line before it names the thread, so the draft is not a reply"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"bpr-main-linux, BPR-1044's survey and BPR-0883's integration pass","found_at":"7eb4aa87","reproduced_at":["7eb4aa87","874dc0ae","78c8ac85"],"first_bad":"cc038748","last_good":"24c9e90c","bisect":"first-parent 88dc42fc..52c3ab60, Linux","related_rfcs":["RFC-BPR-0037"],"related_cards":["BPR-0649","BPR-0657","BPR-0879","BPR-0985"],"run":"bpr-main-linux-20260923","reservation":"docs/issues/_reservations/BPR-1046--BPR-1047--4cc848046b0e.json"}
---

# BPR-1046: application_threaded_reply_contract fails at HEAD: since cc038748 a card's Reply selects the thread's line before it names the thread, so the draft is not a reply

## Defect

`application_threaded_reply_contract` presses Reply on a thread card, as a
reviewer would. It then expects the draft to answer that thread. At HEAD the
draft is created but has no reply target. The course waits in its phase 1
until the run budget expires:

```text
PUBLIC_PATH_FAILED
RUN_BUDGET_EXPIRED:elapsed_ms=40018 budget_ms=40000 over_ms=18 stage=2
THREADED_REPLY_FAILED
THREADED_REPLY_EVIDENCE:00000
THREADED_REPLY_DIAGNOSTIC:phase=0 outstanding=0 diff=1 thread=1 | phase=1 draft=1 reply=- anchorSide=0 anchorLine=29 surfaceSide=0 surfaceLine=29 availability=0 discuss=1
```

`draft=1 reply=-`: a draft exists on the thread's line (29, left side), and
the thread's anchor is current (`availability=0`). But the draft names no
thread. If a reviewer published it, the result would be a new inline comment
on line 29, not a reply in the thread. That consequence is read from the
code. Nothing was published.

The reserved title stated only the symptom. This title adds the cause, which
the bisect and the probe below establish.

## Reproduced at HEAD (bpr-main-linux, 2026-09-23)

WSL2 Ubuntu 26.04, GCC 15.2, Qt 6.11.2, `build/dev` (Debug) at `78c8ac85`.
The row ran through `ctest -j1`, offscreen, with `DISPLAY` and
`WAYLAND_DISPLAY` unset and fresh `HOME` and XDG roots. It failed in 40.35 s
with the lines above, the same as in [[BPR-1044]]'s survey at `7eb4aa87`
and [[BPR-0883]]'s integration pass at `874dc0ae`. No other build or test
ran on the station during it.

## The last green commit (bisect, Linux)

The row passed on Linux in the sweeps at `2b2e5549` (3.79 s) and `88dc42fc`
(3.80 s). A separate worktree built each step with the `dev` preset. Qt
was pinned to the same 6.11.2 prefix at every step through
`BPR_QT_PROVIDER=OFF` and `CMAKE_PREFIX_PATH`. The step built the
application and the network sandbox, then ran the row. A compiler launcher
dropped `-Werror`, because `19a3dfd7`..`f863ce1b` does not build under
GCC 15.2 with it ([[BPR-0975]]'s fix). The bisect followed first parents.

| Commit | Date | Row |
| --- | --- | --- |
| `88dc42fc` | 2026-09-17 | passed, 2.58 s (Qt 6.11.2, so the Qt upgrade is not the cause) |
| `52c3ab60` | 2026-09-23 | failed, 40.34 s |
| `b5fa5246` | 2026-09-21 | failed |
| `bee9a577` | 2026-09-19 | failed |
| `e34649d6` | 2026-09-18 | passed, 2.58 s |
| `10d805f2` | 2026-09-19 | failed |
| `d2282aba` | 2026-09-18 | passed, 2.50 s |
| `8996109f` | 2026-09-18 | passed, 2.61 s |
| `8939de71` | 2026-09-19 | failed |
| `24c9e90c` | 2026-09-18 | passed, 2.53 s |
| **`cc038748`** | 2026-09-19 | **failed: the first bad commit** |

`cc038748` is "feat(rfc0037): integrate first-PR reading surfaces". Its
first parent, `24c9e90c`, passes. Every failing step printed the same
phase-1 diagnostic. [[BPR-0985]] traced a separate regression, in the restoration course, to this same
commit. That card also recorded this row stopping at its 40 s budget on
2026-09-22. Its station's platform is not recorded there.

## The cause, bounded by a probe at HEAD

`cc038748` reordered the Reply action in `workbench_window.cpp` (the
`card_action::reply` case):

- **Before:** it called `ReviewReplyDispatch(aIdentity)` first, then
  `select_thread_line(aIdentity)`. No draft existed when the thread was
  named, so `begin_review_reply` (`src/app/src/github_application.cpp`)
  stored it as `PendingReplyThread`. The draft that the line selection
  then created took it as its reply target.
- **After:** it calls `select_thread_line` first, and dispatches the reply
  only if the selection succeeded. The selection creates the draft, so by
  the time the thread is named a draft already exists. `begin_review_reply`
  then either stores the thread as pending for a later draft, or tries to
  retarget this draft through `SharedDrafts->apply`. In the second case it
  returns silently if that apply is refused or not accepted. Which of the
  two happens was not traced. Both leave `reply=-`.

The comment above the case still describes the old order ("the thread is
named to the composition first, so the draft that follows the selection
answers it").

**The probe.** A temporary, uncommitted edit at HEAD put the dispatch back
before the selection and left everything else alone. HEAD here is
`78c8ac85` plus [[BPR-0885]]'s logging change, which does not touch this
path. The row passed in 2.70 s and
printed `THREADED_REPLY_CONTRACT_OK`. With the file restored byte for byte,
the same build failed again at 40.32 s with the phase-1 diagnostic.

The probe is not the fix. The old order named the thread even when its line
could not be selected. A later, unrelated draft could then have answered it.
`cc038748` dispatches only after the selection succeeds, and otherwise
shows the "Open this thread in code before replying." notice. The fix has
to keep that behaviour.

**Possibly also affected (not observed).** `begin_review_reply` sets the
editor's "Replying to …" label before it retargets the draft. If the
retarget is refused, the label could claim a reply that the draft does not
carry.

## Scope

- Make Reply produce a draft that answers the thread, and keep the notice
  when the thread's line cannot be selected. Two ways to do it:
  - Check that the line can be selected before naming the thread, then
    name it, then select.
  - Make `begin_review_reply` retarget the draft the selection just
    created, and stop it failing silently.
- Correct the comment in the `card_action::reply` case.
- Make the editor's reply label follow the draft's actual reply target.
- Add a check below the composed row that pins the result.
  `workbench_window_tests.cpp` covers the window's side of Reply with a stub
  dispatch: an off-file thread is refused with the notice, and nothing is
  dispatched. No check there or in the host looks at the reply target of
  the draft that follows. [[BPR-0985]] found the same commit breaking
  another behaviour.

## Acceptance

- AC1 `application_threaded_reply_contract` passes on Linux, and prints
  `THREADED_REPLY_CONTRACT_OK` with the full evidence mask.
- AC2 When a thread's line cannot be selected, Reply shows the "Open this
  thread in code before replying." notice and leaves no pending reply that
  a later draft could take. A test proves both.
- AC3 A negative control proves the new check can fail. With the
  pre-`cc038748` order restored, or the reply target dropped, the new check
  fails.
- AC4 The row passes on macOS and Windows. The cause is portable Qt adapter
  code. The row has not been measured on either platform at HEAD.
