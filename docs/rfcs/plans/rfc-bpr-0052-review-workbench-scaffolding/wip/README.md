# BPR-1132 work in progress: the document tab strip

Saved at the owner's wind-down instruction of 2026-09-25, so that main carries
no unfinished product code. The card stays In Progress; its checkpoint is on
[BPR-1132](../../../../issues/30-in-progress/BPR-1132-the-document-tab-strip-in-the-comparisons-header.md)
and the milestone's on
[BPR-1135](../../../../issues/10-backlog/BPR-1135-close-rfc-0052.md).

- **Patch:** `BPR-1132-document-tab-strip.patch` (17 files, SHA-256
  `830375e7239d9328585f0ba6dbbb3758200fa26288f6cb6c76fc37ba3f53eb0c`).
- **Base:** `c0fdd70a` (main). `git apply --check` passes against it.
- **Resume:** from the repository root,
  `git apply docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/wip/BPR-1132-document-tab-strip.patch`,
  then `cmake --build --preset dev`.

## What the patch holds

- `application/document_tabs_projection.hpp` (new): the tab projection and its
  port, split out of `document_tab_host.hpp` so Qt receives only the projection.
- `qt/document_tab_strip.{hpp,cpp}` (new): the comparison's header. A `QTabBar`
  in document mode, names eliding from the left and disambiguated by path, a
  pill for a tab's own pair, the active tab and a "+N" menu when the tabs do not
  fit, a "+" that duplicates the active tab, each tab's menu (Duplicate Tab,
  Close Tab, Close Other Tabs, Use the Window's Comparison, Make New Tabs Use
  This Comparison), and the comparison's status at the right while it says more
  than "ready". Every gesture leaves as a core tab intent.
- `diff_workbench`: `set_status_shown`, `set_status_observer`,
  `status_notable`; the window hides the status row and the strip carries it.
- `epoch_line::dates_of` for a tab's pair; `github_conversation_panel::
  current_head_thread_count` for the older-pair notice; `files_tree_view`:
  Cmd-click (Ctrl-click) and "Open in New Tab" report `open_in_new_tab`.
- `workbench_window`: the strip above the comparison; Next Tab and Previous Tab
  (`Meta+Tab` and `Meta+Shift+Tab` on macOS, `Ctrl+Tab` and `Ctrl+Shift+Tab`
  elsewhere) in the View menu and Close Tab (`Ctrl+W`, Cmd-W on macOS) in the
  File menu, all window-owned; the older-pair notice in the Comments tab ("No
  threads on Feb 4 → Feb 9. Two threads are on later updates of this file." and
  Show them, which activates a tab of the file on the window's comparison, or
  returns this tab to it, or opens Discuss); Comment disabled off the current
  head; the tab stops in the declared focus order.
- `github_application.cpp`: a `document_tab_host` per window. Its loads are
  presented through the epoch and workbench hosts: a load already on screen
  completes at once (the reviewer's own file and comparison choices, reflected
  into the active tab); any other is driven by the stages Resume uses
  (`core::window_reading_restoration`), then completed or refused for the load
  in force. Overview and Discuss show the window's comparison, and Read and
  Review return the active tab's pair. Tabs persist with the window's
  destination (`accepted_tabs`), Resume restores them after the reading, a new
  destination resets them, and shutdown waits for the host. The UIforETW
  capture writes schema v7 with the strip's rectangle, and the floors become
  872 folded and 838 open on macOS (measured on this patch), 876 and 842 on
  Linux (derived), 884 and the owner's 850 on Windows.
- `application_uiforetw_selected_diff_contract.cmake`: the same floors, and
  the strip asserted directly under the review bar with the comparison beneath.

## State on the patch (macOS 26.6.2 arm64, `build/dev` Debug)

- `cmake --build build/dev` succeeds.
- Pass: `qt_workbench_window_scale_100`, `qt_workbench_focus_order_scale_100`,
  `application_uiforetw_selected_diff_contract` (872 folded, 838 open),
  `application_setup_destination_public_path` (after the fix that returns only
  the tab's comparison when leaving Discuss), `application_github_public_path_uiforetw_74`,
  `application_diff_workbench_public_path`, `application_workbench_composition_contract`,
  `qt_composed_github_scale_100`, `application_threaded_reply_contract`,
  `application_review_submission_composition_public_path`,
  `application_profile_multi_window_public_path`,
  `application_agent_finding_composition_contract`.
- Fail, as expected until resealed: `application_review_round_trip_composition_contract`
  and `application_composition_contract` (the `github_application.cpp` pins).

## What remains for the card

1. Qt tests: the strip (names, pill, elision, the width floor's count, the
   menu, "+", accessibility, renders at 100, 125 and 200 percent, narrow and
   wide), the tab keys' uniqueness, Cmd-click and Open in New Tab, the
   older-pair notice and Show them, and Comment off the current head.
2. The composed course: new phases on the review-restoration public path's
   binary (a tabs seed and a tabs restore under a new contract script) that open
   a file in a new tab, compare two pairs of one file in two tabs, close one,
   restart, Resume, and find the tabs back exact.
3. The reseal of the two pins with a review recorded in the contract.
4. clang-format and `tidy.py --changed`; the workbench specification; the
   card's record, owner asks and owed runs; the width sweep; the integration
   pass.
