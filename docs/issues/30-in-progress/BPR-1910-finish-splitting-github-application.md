---
schema: "implement-rfc.card.v1"
id: "BPR-1910"
title: "Finish splitting github_application.cpp"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1909"]
priority: "P2"
metadata: {"baseline":"72f469cef","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass."}
---

# Finish splitting github_application.cpp

Round 6, the last for `github_application.cpp`. What remains besides the
security-pinned review-mutation span is the startup implementation, the public
`github_application` members and the startup fixture accessors, under an
include list sized for the 42,000-line file it used to be.

## Acceptance

- AC1: Move the startup implementation and `github_application::start`, the
  desktop-integration members and the window lifecycle and status members,
  unchanged, into their own sources; the startup fixture accessors join the
  test-only fixture files.
- AC2: The root keeps the automated-test boundary reader, the provider
  using-declarations and the pinned mutation span, and includes only what that
  code uses (platform-guarded and contract-required includes stay).
- AC3: Preserve behavior: full dev and tests-off builds; composition (source
  and Release), round-trip, startup, profile and window courses; format and
  documentation gates.
- AC4: Land the change, the completed card and a journal line on local main.

## Result

Source `71abe5309`; `github_application.cpp` goes from 3,161 to 1,502 lines
(42,564 when this refactor began). New files (lines): `application_desktop.cpp` 454, `application_startup.cpp` 443, `application_startup.hpp` 99, `application_windows.cpp` 604, `tour_owner_replies.cpp` 14, `tour_owner_replies.hpp` 14, `fixture_startup.cpp` 91.

The root now holds the automated-test boundary reader, the provider
using-declarations, and the security-pinned review-mutation span
(`compose_review_mutation` through `refresh_review_projection`, about 1,370
lines) as out-of-line definitions of the window composition. Its include list
drops from about 290 to 43: an include stays when the remaining code references
something it declares (in the tests-on and tests-off parses), when it defines a
`BPR_` macro the code tests, when it is platform-guarded, or when a contract
requires it (`live_review_mutation_provider.hpp`, `github_cli_runtime.hpp`).
`tour_owner_error` and `assist_refusal_code` get their own small pair because
both the desktop routes and `open_window` answer waiting tour requests with them.

Checks on macOS arm64: full dev build; tests-off composition build; clang-format
22; and the 361-row integration pass over every `application_*`, `qt_composed_*`,
architecture and live-mutation row: 349 pass. The 12 others are the same as in
[[BPR-1909]]: `application_profile_window_review_restoration_public_path` times
out under parallel load and passes alone (159.95 s); six
`application_large_review_*` rows are nongating Debug timing benchmarks; and six
fail identically at round 1 (`2fc1cb2a3`). Windows and Linux were not built.
