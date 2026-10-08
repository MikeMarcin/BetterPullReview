---
schema: "implement-rfc.card.v1"
id: "BPR-1333"
title: "Profile layout course crashes in Overview: it clicks the removed Read full description button"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"BPR-1212 slice 2, 2026-10-08: application_profile_window_layout_public_path's seed crashes clicking overviewReadDescription in Overview","reservation":"docs/issues/_reservations/BPR-1333--BPR-1333--b01c1e057218.json","first_bad_commit":"1f81ec5b4","related_cards":["BPR-1314","BPR-1212","BPR-1332","BPR-0708"]}
acceptance: [{"criterion":"BPR-1333:reproduce","outcome":"pass","revision":"6a818923f","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev -R '^application_profile_window_layout_public_path$' --output-on-failure (macOS arm64 Debug)","detail":"Failed with \"seed failed (-11)\". The seed phase run alone exits 139 (SIGSEGV)."}},{"criterion":"BPR-1333:stack","outcome":"pass","revision":"6a818923f","evidence":{"kind":"command","command":"lldb --batch -o 'process attach -p <seed pid>' -o continue -k 'bt 4' -k 'frame select 1' -k 'frame variable read back' (seed launched offscreen with a private data root)","detail":"EXC_BAD_ACCESS (address=0x20) at QtWidgets QAbstractButton::click() + 16, called from resize_summary at profile_window_layout_public_path_tests.cpp:127 (read->click()), from main:240 (the seed's first resize_summary). frame variable: read = nullptr, back = nullptr. The fault is in the course, on a null pointer; the product does not crash."}},{"criterion":"BPR-1333:bisect","outcome":"pass","revision":"1f81ec5b4","evidence":{"kind":"command","command":"git bisect start origin/main 9cfa5eabc; git bisect run <build test_profile_window_layout_public_path; bad if the course reports 'seed failed (-11)'>","detail":"First bad commit 1f81ec5b4 (Reclaim Overview reader space and copy original Markdown on Select All). Good bound 9cfa5eabc passed in full; 224a2bfd5, af813b34d and 1868e55b7 (its parent) passed in full; 1f81ec5b4, 2499010b0, abe1deae7, e19014c9b and 97a8235f7 crashed. BPR-1331 (166773498), BPR-1313's reseal (1e277adcc), BPR-1089 (9cfa5eabc, good) and BPR-1305 (7289987eb, before the good 224a2bfd5 side) are not the cause."}},{"criterion":"BPR-1333:fix-and-control","outcome":"pass","revision":"ef701b45a","evidence":{"kind":"command","command":"ctest -R '^application_profile_window_layout_public_path$' with the fix; then with only the control's object name put back to overviewReadDescription","detail":"With the fix the course passes (seed, restore, verify, broken). With the old name it fails by name, \"FAILED: composed Overview offers its full-view control\", exit 1 rather than -11. Before the fix (main 6a818923f) it faulted."}},{"criterion":"BPR-1333:rows","outcome":"pass","revision":"ef701b45a","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev -j1 -R '^(qt_pull_request_overview_page|qt_workbench_overview|application_overview_summary_.*|application_nested_overview_folders_public_path|application_overview_feedback_public_path|application_overview_labels_public_path|application_profile_window_layout_public_path)$'","detail":"9 of 9 passed, macOS arm64 Debug, offscreen, before and again after rebasing the fix (3ce5a737b) onto e9f461835 as ef701b45a."}}]
---

# BPR-1333: Profile layout course crashes in Overview

## Defect

`application_profile_window_layout_public_path` failed on main with
`seed failed (-11)`. BPR-1212 slice 2 found it: the seed phase crashed while
clicking `overviewReadDescription` in Overview.

## Cause

First bad commit: 1f81ec5b4, "Reclaim Overview reader space and copy original
Markdown on Select All".

That commit replaced the Overview's **Read full description** and **Return to
Files** push buttons (`overviewReadDescription`, `overviewReturnToFiles`) with
one top-right tool button, `overviewFullView`. It updated the Overview page
tests and `overview_summary_public_path_tests.cpp`, but not this course.
`resize_summary` still looked up the two push buttons. `findChild` returned
null for both, and the course called `read->click()` without checking, so
`QAbstractButton::click` read through a null `this` (address 0x20).

The product did not crash. The fault was in the course, and the Overview's
full view works through its current control: the Overview rows and the
composed summary public path pass.

## Fix

`resize_summary` in `tests/end_to_end/profile_window_layout_public_path_tests.cpp`
finds `overviewFullView` and checks that it exists, is visible and reads
"Enter full view" before clicking it. It then waits for "Exit full view" with
Files hidden, and clicks again to return to Files. Since dbbccdf6e the control
is not checkable, so the course follows its accessible name and not a check
state. If the control is renamed again, the course now fails by name rather
than faulting.

## Not covered

- RFC-BPR-0063 line 107 still says "Keep **Read full description**, **Return
  to Files**". The owner's amendment at the top of that RFC covers the change,
  and the body is the owner's text, so it was not edited here.
- Other courses that dereference `findChild` results without checking them
  were not audited.
- Windows and Linux runs of the course are station work, not acceptance
  criteria here.
