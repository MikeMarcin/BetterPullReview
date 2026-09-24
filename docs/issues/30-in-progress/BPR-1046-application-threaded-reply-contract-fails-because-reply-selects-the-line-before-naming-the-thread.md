---
schema: "implement-rfc.card.v1"
id: "BPR-1046"
title: "application_threaded_reply_contract fails at HEAD: since cc038748 a card's Reply selects the thread's line before it names the thread, so the draft is not a reply"
status: "In Progress"
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

## Traced on macOS, and fixed in the product (bpr-main-mac, Mac batch 5, 2026-09-23)

Station `bpr-main-mac` (Apple Silicon, AppleClang, Qt 6.11.2, `build/dev`
Debug), claimed at `631654ec` after Linux released it. At `0ca739b2` the row
failed here exactly as on Linux: `RUN_BUDGET_EXPIRED:elapsed_ms=40023 …
stage=2`, then `THREADED_REPLY_FAILED` and the same `phase=1 draft=1
reply=-` diagnostic.

### Which of the two cases: neither

Temporary prints (all removed since) in the Reply case,
`select_thread_line`, `begin_review_reply` and `diff_workbench::select_line`
showed that `begin_review_reply` is **never called**:

```text
line side=1 left_current=0 right_current=1 line=42
select_line_entry result=0 index=0 visible=0
window selected=0 dispatch=1 editor=1
```

The thread is on the right side at line 42, and the right endpoint is
current. But the comparison surface has **no result** when Reply is pressed,
so `select_conversation_line` returns false. The Reply case then shows the
notice and dispatches nothing. The course moves on when the application's
`LastSelectedDiff` is set. That happens when the selected-diff *request* is
published (`publish(workbench_selected_diff_publication)` calls
`request_diff` and sets it). The diff itself arrives later. So at the press
the thread's own file is still loading, and its line cannot be selected yet.
The draft the phase-1 line reports, at left:29, is not Reply's. It comes from
the default first-change selection when the comparison arrives.

**Why the old order passed, and why restoring it would be wrong.** Before
`cc038748`, Reply named the thread first. No draft existed yet, so
`begin_review_reply` held the thread in `PendingReplyThread`. The next draft
took it: the one at left:29, created by the default selection. The course
passed with a reply anchored on an unrelated line, answering a thread on
right:42. That is the exact hazard the card names. The probe that "restores
the old order" passes for this reason, so it is not a fix.

### The fix (product, both halves)

**Window** (`src/adapters/qt/src/workbench_window.cpp`, ~5164-5205, and the
`card_action::reply` case ~1341):

- Reply still selects the thread's line first and names the thread only once
  that succeeds (`begin_thread_reply`). The refusal notice is unchanged.
- New: when the line cannot be selected **because the thread's own file is
  still loading**, Reply waits instead of refusing
  (`thread_comparison_loading`: the thread is current, the surface's context
  is its file, and no result has arrived). The wait is `DeferredReply`, one
  thread identity held by the window.
- It resumes at the one point where a comparison has arrived and the
  endpoints are placed: the end of `set_conversation_endpoint_placement`,
  which the composition calls when it asks for a comparison and again when
  the comparison arrives. The first comparison to arrive decides. If it is
  the thread's own, the line is selected, the thread named and the
  composition opened. Any other file gets the notice. Nothing waits past it,
  and any other card gesture drops the wait.
- The comment in the Reply case now says what the code does.

**Host** (`src/app/src/github_application.cpp`, `draft_on_thread_line` and
`begin_review_reply`, ~8713-8790):

- `begin_review_reply` retargets only a drafting draft anchored on the
  accepted thread's own line. The side and the line come from the thread
  record, and the check matches the defect course's `anchorMatchesThread`. A
  draft on any other line is left alone.
- **`PendingReplyThread` is gone.** It existed for the old order. With the
  line selected first, it could only ever hand a thread to a later draft on
  some other line, which AC2 forbids. `adopt_review_selection` no longer
  passes it to `SharedDrafts->select`.
- The editor's "Replying to …" label is described only after the retarget is
  accepted. A refused retarget leaves no label naming a reply the draft does
  not make. Its visibility already followed the shown draft's reply target
  (`render_draft`).

### Checks (AC2, AC3)

`test_a_reply_waits_for_its_threads_own_comparison` in
`tests/adapters_qt/workbench_window_tests.cpp` is run by
`qt_workbench_conversation_reveal` and by the full `qt_workbench_window_*`
runs. It drives the window as the composition does: endpoints placed at the
request, the load, then the endpoints placed again at arrival. Reply is
pressed on the feed card while the thread's file loads.

- **The thread's file arrives.** At the press nothing is named and nothing
  is refused. On arrival the dispatch fires once. It records the surface's
  selection at that moment: right side, line 21 (0-based), which is the
  thread's line. No notice.
- **Another file arrives.** No dispatch, and the notice is shown. The
  thread's own file arriving afterwards names nothing, so no reply is left
  waiting (AC2).
- cc038748's existing check still holds: an off-file thread is refused with
  the notice, with nothing dispatched and the selection unchanged (AC2).

**Negative controls** (AC3). Each was a temporary edit, restored afterwards
and re-hashed; `grep -c BPR1046_` gives 0 in `src` and `tests`.

| Break | Result |
| --- | --- |
| The pre-`cc038748` order in the window (name, then select, never wait) | `qt_workbench_conversation_reveal` fails at cc038748's own off-file check first. With the new test called first, it fails on its own: `FAILED: Reply waits while its thread's own comparison loads: nothing is named and nothing refused` |
| HEAD's order with no wait | `FAILED: Reply waits while its thread's own comparison loads: nothing is named and nothing refused` |
| The host drops the reply target (`begin_review_reply` returns before the retarget) | `application_threaded_reply_contract` fails: `RUN_BUDGET_EXPIRED:elapsed_ms=40022`, `THREADED_REPLY_FAILED`, `phase=1 draft=1 reply=- anchorSide=1 anchorLine=41 surfaceSide=1 surfaceLine=41`. The window's half put the draft on the thread's own line; the retarget is what makes it a reply |

### Verification (macOS, AC1's row and AC4's macOS half)

| Check | Result |
| --- | --- |
| `cmake --build --preset dev` | Exit 0, no `warning:` line |
| `ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_threaded_reply_contract\|qt_workbench_conversation_reveal\|qt_workbench_window_scale_100\|qt_composed_github_scale_100\|qt_github_conversation_panel\|qt_conversation_cards_view\|qt_review_draft_editor\|application_shared_draft_application_host\|application_review_submission_composition_public_path\|application_review_round_trip_composition_contract\|application_defect_composition_contract\|application_review_ledger_composition_contract)$'` | 12/12 passed. `application_threaded_reply_contract` passed in 22.61 s, and the contract requires `THREADED_REPLY_EVIDENCE:11111` before `THREADED_REPLY_CONTRACT_OK` |
| `application_composition_contract` (the tests-off Release graph; `begin_review_reply` is production code) | Pass, 223.97 s |
| clang-format 23.1.1 on the four C++ files | Clean |
| `_coding-standard/scripts/tidy.py` on the three translation units | No finding on a changed line. The four it reports (`github_application.cpp:8194`, `workbench_window_tests.cpp:8922`, `home_view_state.hpp:35`, `dif_runtime.hpp:1`) are on untouched lines |

### Pinned-code review and reseal

`github_application.cpp` is in the round-trip contract's pins. The pins
last matched at `a782886d` (BPR-1037), and nothing else touched the pinned
scope since. The change reviewed is the host half above. It issues the same
draft `edit` with `ChangeReplyTarget` as before, now only for a draft on the
thread's line, and it removes the pending-thread path. No credential,
provider, vault, transport or mutation path changes. The mutation-branch pin
is unchanged (`a7e2081e…d1e8a`). New values:

- full application
  `0eccd8109e01a2b9a5e3b1b9698d77f5ea2f5a66f60bbe8e29cd997ffc344470`;
- production manifest
  `52f6a64716e5d97f986382685d33503668973d5892cde12d2f491f490edaf62a`.

### Acceptance state

- **AC1** (the row passes on Linux with the full evidence mask): passes on
  macOS. **Linux is owed.**
- **AC2** (the notice, and no pending reply a later draft could take, proven
  by a test): pass. The window test covers both, and the host no longer has
  a pending path at all.
- **AC3** (a negative control proves the new check can fail): pass, the
  three breaks above.
- **AC4** (the row passes on macOS and Windows): **macOS passes; Windows is
  owed.**

### Still needed

- **Linux (bpr-main-linux):**
  `cmake --build --preset dev && ctest --test-dir build/dev -j1 --output-on-failure -R '^(application_threaded_reply_contract|qt_workbench_conversation_reveal|application_review_round_trip_composition_contract)$'`.
  Expected: 3/3, and the round-trip row confirms the resealed pins.
- **Windows (bpr-main-win):**
  `cmake --build --preset windows-dev && ctest --preset windows-dev -j1 --output-on-failure -R '^(application_threaded_reply_contract|qt_workbench_conversation_reveal)$'`.
