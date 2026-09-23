---
schema: "implement-rfc.card.v1"
id: "BPR-1027"
title: "Windows: closing a second window can crash the owner in ~implementation deleting Realigner (0xC0000409 CFG fast fail)"
status: "Blocked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"BPR-0980 native activation course, bpr-main-win close-probe","found_at":"196296d0","reproduced_at":["c52f5907","196296d0"],"related_cards":["BPR-0980","BPR-1028"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1027--BPR-1028--763d93af262e.json"}
blocked_by: "not-reproducible"
---

# BPR-1027: Windows: closing a second window can crash the owner in ~implementation deleting Realigner (0xC0000409 CFG fast fail)

## Defect

On the Windows station, closing a window that Profiles > New Window opened,
while the first window stays open, ends the whole application with
`0xC0000409` (`-1073740791`). It happened in 5 of 5 targeted probes. The
window was active in some of them and inactive in others, and it was closed
3 s or 30 s after it opened. The same happened with product source `c52f5907`
and with `196296d0`, so the [[BPR-0980]] foreground correction did not cause
it. In the full [[BPR-0980]] course, which opens three windows and closes
each one, it happened in 1 of 5 runs, on the first close
(`observation-fix-1.json`). The other four courses exited 0. The records
are station logs under `build/logs/bpr-0980-main-win/` and are not
committed.

A debugger attached to the owner (the station's crash catcher,
`close-probe-3.json`) caught the second-chance exception:

```text
EXCEPTION 0xC0000409 first_chance=0 param0=0xA   (FAST_FAIL_GUARD_ICALL_CHECK_FAILURE)
#03 std::default_delete<bpr::application::agent_realignment_port>::operator()   memory:3338
#04 std::unique_ptr<...agent_realignment_port...>::~unique_ptr                     memory:3456
#05 bpr::app::github_application::implementation::~implementation      github_application.cpp:7179
#08 std::_Ref_count_obj2<...implementation>::_Destroy
#17 std::_Tree<...bpr_window_context, shared_ptr<implementation>...>::_Erase_unchecked
#19 github_application::open_window'::<lambda_1>::operator()           github_application.cpp:13686
#23 bpr::qt::event_loop_executor::post'::<lambda_1>                     event_loop_executor.cpp:12
#46 main                                                                 main.cpp:8613
```

Line numbers are those of `196296d0`. At `919e2154` the destructor closes at
`:7189`, `Realigner` is declared at `:13108`, and the `OnClosed` erase is at
`:13697`.

The window's `OnClosed` callback erases the session from `Sessions`. For any
window after the first, that erase drops the last reference. `open_window`
keeps only the first session in `Implementation`, so for the first window the
same erase is not the last one, and courses that close only the first window
never run this destructor from that point. The destructor then destroys its
members in reverse order. When it reaches `Realigner`, the virtual deleting
destructor's indirect call fails Control Flow Guard (subcode `0xA`). The
object's vtable pointer points into readable memory that is not a valid call
target. That means the block was freed and then reused before this delete,
so this delete is the second one. The Debug CRT cannot report it: once the
block has been reused its header is a valid live block again. `Realigner` is
`application::composed_agent_realignment`, 24 bytes, composed with the agent
host. Nothing in the close path uses it. It is most likely the victim of a
double free or a use-after-free of some other same-sized block, rather than
the cause.

## Scope

- Reproduce without the desktop first. An offscreen course through the real
  composition that opens a second window with New Window and closes it while
  the first stays open. If the offscreen course is clean, the remaining
  difference is the `windows` platform: the title-row host, the strip menu's
  nested event loop in which New Window ran ([[BPR-1028]]), or native
  activation.
- Find the first free of the block. The Debug CRT's delayed-free mode
  (`_CRTDBG_DELAY_FREE_MEM_DF`, with `_CRTDBG_CHECK_ALWAYS_DF` or a check at
  each teardown step) keeps freed blocks from being reused. The second free
  then asserts at the real culprit instead of failing CFG in a victim. Do not
  commit it as a product switch.
- Fix the cause in the layer that owns it, and pin it with the course.
- State whether macOS and Linux share the defect. The mechanism is
  cross-platform C++ unless the culprit is Windows-only.

## Acceptance

- AC1 The cause is named with evidence: the first free, its owner and why it
  happens twice.
- AC2 A course that closes a New Window window while the first stays open
  runs on Windows and fails without the fix. With the fix it passes N of N
  (N ≥ 5), and the owner exits 0 at the end.
- AC3 The [[BPR-0980]] close probe on the real Windows desktop passes N of N
  (N ≥ 3): New Window, close the new window, the first window survives, and
  its own close exits 0.
- AC4 macOS and Linux run the same course and pass.

## Offscreen: the course is clean in every shape (bpr-main-win, 2026-09-23)

`application_profile_second_window_close_public_path` is the new course
(`tests/end_to_end/profile_second_window_close_public_path_tests.cpp`). It
opens the first window, then five times opens a second window with New
Window, waits 250 ms, closes it with its own `close()`, and waits until the
session has finished its shutdown. It checks that the first window stays open
each time, and it ends with Quit. The data root sits one owned level down,
like [[BPR-0875]]'s courses. Without that, the agent workspace recovery
refuses, and the agent host with its realigner is left out of the
composition.

On Windows it is registered uncontained and pinned in
`scripts/verify_test_network_isolation.py` as `dos-volume-name`. That was
measured: run contained, it stops at "FAILED: the test root's parent is
owner-private", the same `make_storage_root_owner_private` refusal as
[[BPR-0810]]'s rows. `test_network_isolation_manifest` and
`test_network_isolation_structure_contract` pass with the pin.

Three options move the course closer to the desktop probe. None of them
reproduces the crash offscreen:

| Run, offscreen, at `5670bc68` plus this course | Result |
| --- | --- |
| Fixture composition (the registered row) | 5 of 5 closes clean, owner quits 0 |
| `--shipped-composition`: `profile_desktop::start` with no fixture, the graph `main.cpp` starts | clean |
| `--through-strip-menu`: New Window through the strip menu with accessible invokes, against the fixed `popup()` menu | clean |
| The same against the old stack `exec()` menu (pre-[[BPR-1028]] source, built locally) | clean |
| `--walk-accessible`: accessibility on, and all 1,098 accessible interfaces of every window walked before each close, as a UI Automation descendant search does | clean |
| All three together | clean |
| Local diagnostic build with the Debug CRT's `_CRTDBG_DELAY_FREE_MEM_DF`, `_CrtCheckMemory` before and after every close, a report hook and a free-stack ring (not committed) | clean: no double free and no write into a freed block |

The last row matters most. Delayed free keeps every freed block out of reuse
and poisoned, so a double free or a stale write would have been reported at
the culprit whether or not a later reuse lined up. Offscreen, nothing
happens. What the desktop probe had and none of these runs has is the real
`windows` platform: native windows, the [[BPR-0653]] title-row host with its
native event filter, UI Automation's providers, and native activation. So
the next step needs the desktop:

1. The close probe at the current head, with the [[BPR-1028]] menu, 3 times.
   It shows whether the crash survives the menu change.
2. This course on the `windows` platform
   (`QT_QPA_PLATFORM=windows`, `--shipped-composition`, then with
   `--through-strip-menu --walk-accessible`). It shows whether a scripted
   native close reproduces the crash.
3. If either one reproduces, the local diagnostic build of this course under
   the same conditions. Delayed free turns the crash into a report at the
   first double free, or into an access violation in the `Realigner` delete
   together with the recorded stacks that freed that block.

## On the real desktop: it no longer reproduces, not even from the source that crashed (bpr-main-win, 2026-09-23, cleared)

All runs were on JaegAW (Windows 11 10.0.26200, session 1, ordinary user) with
`build/dev` Debug (MSVC 19.51, Qt 6.11.2) and a fresh owner-private root each.
The close probe is the [[BPR-0980]] observer's `close-probe` with the crash
catcher attached: New Window through the strip menu with UI Automation, then
the new window's own close 2 s later, then the first window's close. The
records are station logs under `build/logs/bpr-1027-main-win/`.

| Build (18:37Z to 18:51Z) | Close probe | Other |
| --- | --- | --- |
| `2a5580d6` (`8351faa4…`) | 3 of 3 clean: the first window survives, the last close exits 0, the catcher sees `exit_process code=0x00000000` | The second-window course on the `windows` platform passes in three shapes (fixture; `--shipped-composition`; with `--through-strip-menu --walk-accessible`): 15 of 15 closes |
| `2a5580d6` with only [[BPR-1028]]'s menu change reverted (`68732bab…`, local) | 3 of 3 clean | |
| `2a5580d6` with only [[BPR-0817]]'s `14a18b45` product change reverted (`c040b5ba…`, local) | 3 of 3 clean | |
| **`c52f5907` product source**, the source that crashed 5 of 5 this morning (`c71003dd…`, local) | **3 of 3 clean** | No `cmake/`, top-level CMake or preset differs between `c52f5907` and `2a5580d6` |
| `2a5580d6` (`a00f28be…`, rebuilt after the trials) | | The committed [[BPR-0980]] activation course 2 of 2: three windows, each window's own close, exit 0 |

So the fault is not a function of the product source alone. The same source
and build configuration crashed in 5 of 5 targeted probes between 16:29Z and
16:41Z, in builds `b434be9d` and `47164b2a`. It is clean in 3 of 3 now, and
12 of 12 across four source variants.

**Frontier.** A condition present on the station between 16:29Z and 16:41Z
is absent now. The only recorded difference is the shared desktop. That
morning's probes ran while a [[BPR-0975]] WSLg course was starting and
running on the same desktop: [[BPR-0980]]'s "Shared desktop" note records
two foreground grabs at 16:32Z and 16:38Z. Today's runs were on a desktop the
coordinator had cleared, with the Linux station finished. That is a
correlation, not a cause. The offscreen results (clean with the Debug CRT's
delayed free) point the same way: the fault needs something from outside
the process, such as a native activation or focus change at the moment of
the close. The full course's one crash in five was also during that window
(`observation-fix-1.json`).

**What would move it.** Run the close probe on this station deliberately
while a WSLg course runs on the same desktop. That breaks the station's
foreground-hold protocol, so it has to be arranged by the coordinator with
the Linux station. If it reproduces, run the local diagnostic build of the
new course (`--crt-diagnostics`, delayed free plus the free-stack ring) under
the same condition. That turns the second delete into a report at its first
free. Until then there is nothing to fix. The destructor path the crash
named is now covered by `application_profile_second_window_close_public_path`
on every platform.

Parked on that arrangement: Blocked, `blocked_by: concurrent-desktop-reproduction`.

## The overlap run, and where the card stands (bpr-main-win, 2026-09-23)

### The whole record

**(a) The crash as it was found.** Between 16:29Z and 16:41Z on JaegAW, the
[[BPR-0980]] close probe crashed the owner in 5 of 5 runs, active or not, 3 s
or 30 s after the new window opened. It ran on builds `b434be9d…`
(`196296d0` plus the foreground change) and `47164b2a…` (`c52f5907` product
source). The failure was `0xC0000409` (`-1073740791`), a Control Flow Guard
fast fail with subcode `0xA`, deleting `Realigner` in
`github_application::implementation::~implementation`
(`github_application.cpp:7179`), reached from the `OnClosed` erase in
`open_window` (`:13686`). Both lines are at `196296d0`; the full stack is
under Defect above. The full [[BPR-0980]] course crashed in 1 of 5
(`observation-fix-1.json`). A [[BPR-0975]] WSLg course was starting and
running on the same desktop during that window.

**(b) Clean since then.** No crash in 27 runs without overlap between 18:37Z
and 18:51Z, and none of the 10 overlap closes that completed:

| Run | Clean |
| --- | --- |
| Close probe at `2a5580d6` | 3 of 3 |
| The same with [[BPR-1028]]'s menu change reverted | 3 of 3 |
| The same with `14a18b45` reverted | 3 of 3 |
| **`c52f5907` product source**, which crashed 5 of 5 that morning | **3 of 3** |
| Second-window course on the `windows` platform, three shapes | 15 of 15 closes (3 runs) |
| [[BPR-0980]] activation course at `2a5580d6` | 2 of 2, exit 0 |
| Overlap, 19:04:29Z to 19:05:17Z, worktree at `7bf7de5b` plus local diagnostics (exe `a28a3e8d…`), a WSLg BetterPullReview window up, `wsl_course_processes_at_start=4`: diagnostic course, shipped composition, strip menu, accessible walk | 5 of 5 closes, `--crt-diagnostics` |
| Overlap, the same: diagnostic course, fixture composition | 5 of 5 closes, `--crt-diagnostics` |

**The three overlap close probes never reached a close.** Their exit codes
read 0 because that copy of the observer does not turn its recorded errors
into an exit code. The WSLg window,
"BetterPullReview — Default profile — BetterPullReview (Ubuntu)", held the
foreground throughout (`foreground_left_with`):

- Probe 1 launched the owner, but the posted click opened no Application
  menu ("menu Profiles did not appear"). The observer killed the owner as a
  leftover.
- Probes 2 and 3 stopped before launching: "launcher lost the foreground".

So the UI Automation close probe, the exact shape that crashed, has **not**
been run under overlap. Under overlap only the in-process course ran, closing
through `QWidget::close()` on the real `windows` platform.

**(c) Realigner single-free diagnostics.** The overlap courses ran with the
Debug CRT's delayed free. With delayed free, a second delete reads a
dead-land vtable and faults instead of reusing the block. Each session and
its Realigner was registered, the first free of each was recorded, and every
session destructor reported on entry whether its Realigner had already been
freed. In both logs (`overlap-course-*-diag.log`, station logs):

- 6 sessions registered, 6 session destructors, and `freed=0` for the
  Realigner at every one;
- no access violation, no CRT report, no `_CrtCheckMemory` failure before or
  after any close;
- the Realigner reported at the course's end shows exactly one free, from its
  own `~implementation` (`std::default_delete` → `~unique_ptr` →
  `~implementation`).

There is no double free on the path the crash named, in any run that
reached it.

**(d) Reopen trigger.** Any `0xC0000409`, or any other crash, of the owner
when a window other than the first is closed. That covers a person's close,
UI Automation, a course or a full activation course. Open a new card that
links here, with the crash catcher's stack and the conditions at the time.

### Disposition: not reproducible, held Blocked rather than Done

The board has no terminal lane for "not reproducible". Done needs passing
records against this card's acceptance, and AC1 (cause named) and AC2 (a
course that fails without the fix) cannot pass. Reaching Done would mean
rewriting the acceptance to "not reproducible, guarded". The board rules
allow that only as a recorded amendment by the owner, not to satisfy a
validator.

The instruction to finish as Done rested on the overlap run being "5 of 5
clean". That holds for the two courses but not for the three probes, which
never reached a close. So the card stays **Blocked** with
`blocked_by: not-reproducible`. The amendment is prepared, and applying it is
one step:

> *Amendment (to be confirmed by the owner or integrator):* AC1 and AC2 are
> replaced by: the crash is shown not reproducible on the Windows station
> across the recorded conditions, including the exact crashing source and a
> concurrent WSLg desktop, with the original evidence kept (a to c above).
> `application_profile_second_window_close_public_path` is the regression
> guard for the destructor path on every platform (Windows: passes,
> uncontained, pinned). AC3 is replaced by the reopen trigger (d). AC4 (macOS
> and Linux run the course) stays, for those stations' next sweeps.

Once confirmed, the card finishes as Done with the guard as its deliverable.
Nothing else is left to run unless the trigger fires. The UI Automation close
probe under overlap, the one experiment still open when the above was written,
has since been run (below).

## The UI Automation close probe under overlap: 5 real closes, no crash (2026-09-23)

The Linux station held one plain WSLg BetterPullReview window up, with no
activations. The coordinator ran the staged probe twice. The probe's build was
a detached worktree at `1de0ca9a` plus local diagnostics (exe `b4fc27e5…`,
never committed). Its harness was made loud before the runs:

- The observer exits nonzero on any recorded error.
- It checks and records who holds the foreground before the launch and again
  before the close, with the window title.
- It records whether the close was actually performed: the new window gone,
  or the owner exited.
- An evaluator turns each record into `FOREGROUND_OK`, `CLOSE_PERFORMED`,
  `CRASH` and an exit code.

On the three archived overlap probes above, which had read as exit 0, it now
gives `CLOSE_PERFORMED=no`, and on the morning's crash record it gives
`CRASH=yes code=-1073740791`.

| Run | Probe | Heap diagnostics | Foreground | Close performed | Result |
| --- | --- | --- | --- | --- | --- |
| `uia`, 19:16:08Z to 19:17:09Z | 1 | no | lost before launch, to the WSLg window | **no** (the harness failed loudly, exit 1) | not counted |
| | 2 | yes | the new window | yes | clean: first window survived, last close exit 0, catcher `0x0` |
| | 3 | yes | the new window | yes | clean, the same |
| `uia2`, 19:17:15Z to 19:18:21Z | 1 | no | the new window | yes | clean, the same |
| | 2 | yes | the new window | yes | clean, the same |
| | 3 | yes | the new window | yes | clean, the same |

`FOREGROUND_AT_START` was the WSLg window both times, and
`wsl_course_processes_at_start=1`. So these are **5 real UI Automation closes
of a New Window window under overlap, 0 crashes**: 1 on a plain heap and 4
with the heap diagnostics. The `uia` run also ran the diagnostic course on the
`windows` platform: 5 of 5 closes, exit 0.

**Realigner diagnostics from these runs.** The four app logs are
`uia{,2}-probe-{2,3}-app-diag.log`, station logs. In each, the first window's
session and the new window's session each registered with their Realigner.
Both session destructors reported `freed=0` for their Realigner: the new
window's at its UI Automation close, and the first window's at the last close.
No session was freed before its destructor, and there was no access violation,
CRT report or heap-check failure. The course log (`uia-course-diag.log`)
matches: 6 sessions, 6 destructors, `freed=0` at every one, nothing else. In
every run that reached it, the destructor the crash named deletes a Realigner
freed exactly once.

The totals are now these:

- 27 clean runs without overlap, including `c52f5907`'s product source.
- Under overlap, 10 course closes and 5 UI Automation closes, every one
  clean.

The disposition does not change. The card stays **Blocked**
(`not-reproducible`), and the amendment above is still pending. The reopen
trigger stays (d).
