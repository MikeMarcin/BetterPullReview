---
id: "RFC-BPR-0076"
title: "File and change navigation enters Review"
status: "Auto-Verified"
depends_on: []
metadata: {"rfc_base_sha":"d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1","mandate_revision":"file-change-navigation-r1","milestone_plan":{"id":"file-change-navigation-v1","path":"docs/rfcs/plans/file-change-navigation-v1/milestone-plan.json"}}
---

# File and change navigation enters Review

## Execution status

Mandate `file-change-navigation-r1` adopts the draft and settled owner decisions.
Immutable RFC_BASE_SHA: `d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1`. The
[live import](assets/file-change-navigation-v1.LIVE_IMPORT.md) preserves source bytes, and the
[one-milestone plan](plans/file-change-navigation-v1/milestone-plan.json) covers all criteria.
AC01–AC05 pass on `aa3d71fe4`: all four retained navigation actions enter
Review before stepping its exact active-tab context, including bounded entry.
[[BPR-1417]] records acceptance. The [verification ledger](../verification/RFC-BPR-0076.md)
binds the composed/window evidence, negative control and baseline tidy limits.
No required human/native/independent acceptance remains.

## Problem and grounded current behavior

On 2026-10-12 the owner requested that Next file and Next change enter Review
from Overview, Discuss or Threads. They explicitly chose to enter Review even
at the end, keeping the current position, and clarified that Previous file and
Previous change should work the same way.

The four actions share `dispatch_navigation` in the
[window](../../src/adapters/qt/src/workbench_window.cpp). It dispatches
file or change navigation without entering Review; a bounded command instead
shows its reason for three seconds and sends no navigation. The
[window tests](../../tests/adapters_qt/workbench_window_tests.cpp)
explicitly expect Previous/Next change beneath Discuss to keep Discuss visible
and retain the selected conversation. This is inspected code and test behavior,
not a new runtime observation.

The [workbench contract](../ux/workbench-specification.md) already defines
bounded, always-enabled navigation, remembered Review drawers and document tabs.
Overview, Discuss and Threads use the window's comparison, while Review uses
the active document tab's exact pair. Simply revealing Review after stepping
the hidden comparison could therefore navigate the wrong material. The
[composition](../../src/app/src/github_application.cpp) schedules tab
presentation on a modality change and already handles file gestures racing
that restoration.

## Proposed behavior and boundaries

### Enter Review and navigate its context

With an active pull request, invoking **Previous file**, **Next file**,
**Previous change** or **Next change** from Overview, Discuss or Threads enters
Review and performs that command once. Toolbar clicks, menu activation and
the existing keyboard shortcuts share this behavior. Invocation while already
in Review performs ordinary navigation without reapplying its drawer preset.

The command operates on the file, exact comparison and reading position that
Review would restore on entry. Resolve that context before evaluating the
command's bound or applying its step; do not navigate the other modality's
window comparison and then restore a different tab over the result. File
navigation uses the existing actionable Files order and replaces the active
tab's file while retaining its exact pair. Change navigation stays within
that tab's selected file. Sibling tabs retain their views and positions.

With no document tab, keep the existing first-file behavior: Next file can open
the first actionable file on the window's comparison. Other commands use their
ordinary bound, including no open comparison. Entering Review alone does not
create a source tab or choose a file.

### Bounds, loading and retained work

**Settled owner decision:** all four commands enter Review even when the
resolved Review context is bounded. Keep the file, comparison and reading
position unchanged and show the existing exact bound message for three seconds.
Arrows remain enabled for an active pull request. There is no wrap or crossing
into a neighbouring file through change navigation.

If the exact Review comparison needs restoration/loading, retain the command
only for that originating PR, active tab and exact pair; apply it once when
that context is ready. A replacement context cancels the pending step, and
late material cannot overwrite a later gesture. Unavailable material stays
unavailable with its ordinary explanation; navigation never substitutes a
newer pair. Review becomes visible without waiting for successful loading.

Use normal Review entry to restore its saved drawer arrangement, including
hidden drawers. Preserve unsent comments, replies, notes and review summaries,
and retain the other modalities' reading/filter state under their existing
rules. A navigation press does not select an unrelated thread or discard a
draft. Keyboard invocation moves focus to the Review comparison when available;
an empty/unavailable Review keeps focus on a visible navigation control.
Existing editable multi-line shortcut handling remains in force.

### Scope and ownership

This is a pull-request navigation refinement across the existing four
modalities. Home, local Review Cycles, separate composition workflows and
inactive workbenches retain their existing commands. No new control, shortcut,
setting or publication authority is added.

Follow the [workbench architecture](../architecture/system-architecture.md#rfc-bpr-0005-workbench-allocation):
core owns legal transitions and bounds; the host coordinates exact tab context,
loading and stale-result admission; Qt translates gestures and presents the
selected modality, focus and status. Reuse bounded projections and selected-file
loading without a full-review scan or model reset. Internal sequencing is an
implementation choice subject to these observable guarantees. Focused public-path verification owns
`tests/end_to_end/review_navigation_entry_public_path_tests.cpp`, its CMake
registration and the corresponding `scripts/verify_test_network_isolation.py`
manifest entry.

The [proposed amendment](assets/file-change-navigation-v1/proposed-navigation-amendment.source.txt) identifies the
adopted contracts and historical evidence to reconcile during implementation.
There are no remaining material design questions.

## Acceptance criteria

- **AC01:** Each of the four real navigation actions, invoked from each of
  Overview, Discuss and Threads, reveals Review and performs exactly its
  ordinary step when available, through clicks, menus and admitted shortcuts.
- **AC02:** First/last file, first/last change, no-open-comparison and
  no-changes cases reveal Review with the unchanged source position and exact
  three-second bound message. The no-tab first-file case follows “Enter Review
  and navigate its context.” Inactive workbenches remain disabled.
- **AC03:** When an active tab retains an older pair than the window's pair,
  navigation follows that tab's material and preserves sibling tabs. Delayed
  restoration performs one step on the originating context; replacement or
  unavailable material cannot produce a step on a different pair or overwrite
  later navigation.
- **AC04:** Entry restores Review's remembered drawer arrangement, including
  hidden drawers; repeated navigation inside Review does not reset it. Unsent
  work survives the round trip, and returning to discussion retains its
  existing reading and filtering state.
- **AC05:** Keyboard entry leaves focus on visible Review content or its
  visible fallback, and editable multi-line fields retain their native keys.
  Home, local Review Cycles and separate composition retain existing behavior.
