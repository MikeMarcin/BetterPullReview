---
schema: "implement-rfc.card.v1"
id: "BPR-0945"
title: "Resolve existing strict tidy diagnostics in shared Qt and Home headers"
status: "Done"
rfcs: []
depends_on: []
priority: P2
metadata: {"origin":"r23 focused integration","scope":"Unchanged shared-header diagnostics, outside BPR-0941..0943","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency","milestone_plan":{"id":"bpr0945-tidy-headers","path":"docs/rfcs/plans/bpr0945-tidy-headers/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f8b589e7f10811126c1b811ca6f2b28d48738e38","evidence":{"kind":"observation","detail":"Two-header source review: unique standard include guard and unsigned initial mask operand; uint8_t constants and bits 1/2/4/8, all=15 unchanged; no tidy profile/helper suppression or runtime change."}},{"criterion":"AC2","outcome":"pass","revision":"f8b589e7f10811126c1b811ca6f2b28d48738e38","evidence":{"kind":"command","detail":"Canonical _coding-standard/scripts/tidy.py with --warnings-as-errors and Xcode MacOSX.sdk: baseline reproduced both diagnostics (exit 1); repaired diff_workbench.cpp, files_tree.cpp, workbench_window.cpp and epoch_application_host.cpp all pass (exit 0), clang-tidy 23.1.1."}},{"criterion":"AC3","outcome":"pass","revision":"f8b589e7f10811126c1b811ca6f2b28d48738e38","evidence":{"kind":"command","detail":"Six focused test targets built; 7/7 serial offscreen CTest Home/persistence/Dif/composed-window rows pass in 22.27s. Both headers pass clang-format 23.1.1; git diff --check passes."}}]
---

# Resolve existing strict tidy diagnostics in shared headers

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending at the r4 transfer: Canonical strict tidy header diagnostics; no broad suppression or passing tidy result is inferred.
No owner decision, native observation or repair is supplied by this transfer.

The canonical tidy helper, with the proper macOS SDK and warnings promoted to
errors, reports two shared-header diagnostics while checking the r23 touched
translation units: `portability-avoid-pragma-once` at `dif_runtime.hpp:1`, and
`bugprone-signed-bitwise` at `home_view_state.hpp:35`. Both files are byte-unchanged
from entry revision d47a6e18. No changed-line diagnostic was reported. This is
recorded debt, not a passing strict tidy result or authority to broaden the
selected-diff repair.

## Acceptance

- AC1: Reconcile both diagnostics with the coding standard without broad rule
  suppression or behavior changes.
- AC2: Run the canonical helper with warnings as errors on the affected Qt
  workbench and application translation units, with the proper host SDK.
- AC3: Re-run focused Home/Qt compilation and tests appropriate to any change.

## Verified repair — 2026-09-25

Candidate `f8b589e7f10811126c1b811ca6f2b28d48738e38`, based on `9a0b44d5`.

- AC1: Replaced the sole `#pragma once` in `dif_runtime.hpp` with a unique
  path-based include guard. Promoted the first Home mask operand to `unsigned`,
  keeping all subsequent bitwise results unsigned. The public constants remain
  `std::uint8_t`, the four bits remain 1/2/4/8, and `all` remains 15. No rule
  suppression, type/schema change or runtime branch change was introduced.
- AC2: The canonical SDK-aware helper reproduced both warnings before repair
  (exit 1). After repair it passed with warnings as errors on
  `diff_workbench.cpp`, `files_tree.cpp`, `workbench_window.cpp` and
  `epoch_application_host.cpp` (exit 0). The canonical profile and helper are
  unchanged. This is a four-translation-unit result, not a repository-wide tidy
  result.
- AC3: All six selected targets rebuilt successfully. Seven focused CTest rows
  passed in 22.27 seconds: `application_workbench_host`,
  `sqlite_workbench_settings_storage`, `qt_composed_github_scale_100`,
  `qt_composed_home_sign_in`, `qt_workbench_home_discovery_scale_100`,
  `qt_dif_runtime`, and `qt_home_list_scale_100`. Existing tests exercise
  collapsed-panel persistence, unknown-bit rejection, Home gestures, composed
  presentation and Dif runtime. No duplicate regression test was added.

Host: macOS 26.6.2 arm64, AppleClang 21.0.0.21000101, Qt 6.11.2, CMake 4.4.3,
Python 3.14.7, LLVM clang-tidy/clang-format 23.1.1. Both touched headers pass
canonical formatting. Qt tests ran offscreen and serially; no native visual,
Windows or Linux acceptance is claimed. The owner deferred those platform runs.
The [handoff](../../rfcs/plans/bpr0945-tidy-headers/HANDOFF-2026-09-25.md)
records commands, ownership and integration scope. No additional owner decision
is required by this card.

Documentation impact: no semantic impact on UI commands, provider authority,
persistence formats, effects, lifecycle, build selection or Dif assets. Header
inclusion and compile-time integer promotion are the only product edits.
