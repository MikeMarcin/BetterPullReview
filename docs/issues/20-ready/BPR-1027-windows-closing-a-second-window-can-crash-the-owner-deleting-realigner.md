---
schema: "implement-rfc.card.v1"
id: "BPR-1027"
title: "Windows: closing a second window can crash the owner in ~implementation deleting Realigner (0xC0000409 CFG fast fail)"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"found_by":"BPR-0980 native activation course, bpr-main-win close-probe","found_at":"196296d0","reproduced_at":["c52f5907","196296d0"],"related_cards":["BPR-0980","BPR-1028"],"run":"bpr-main-win-20260923","reservation":"docs/issues/_reservations/BPR-1027--BPR-1028--763d93af262e.json"}
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
