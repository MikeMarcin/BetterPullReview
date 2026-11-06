---
schema: "implement-rfc.card.v1"
id: "BPR-1902"
title: "Preserve checked cancellation-summary iterators"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"discovered_by":"BPR-1901","milestone_plan":{"id":"windows-checked-iterator-v1","path":"docs/rfcs/plans/windows-checked-iterator-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"a0ca632a1","evidence":{"kind":"observation","detail":"Native MSVC Debug and Release selected builds pass with the exact two-line source correction at a0ca632a1; baseline a73e25720 failure preserved. Compiler options unchanged.","artifact":"/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-windows-rebuilds-gl2kqegv/baseline-recovered-receipt.json"}},{"criterion":"AC2","outcome":"pass","revision":"a0ca632a1","evidence":{"kind":"observation","detail":"Existing application_github_window_read_host passes natively in Debug (2.8298s) and Release (0.3099s); no assertions changed. Format22 and documentation gate pass.","artifact":"/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-windows-rebuilds-gl2kqegv/baseline-recovered-receipt.json"}},{"criterion":"AC3","outcome":"pass","revision":"a0ca632a1","evidence":{"kind":"observation","detail":"Source correction a0ca632a1 fast-forwarded to main. This guarded finish includes the verified one-line journal entry; correction is common to both pending performance measurements.","artifact":"/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-windows-rebuilds-gl2kqegv/baseline-recovered-receipt.json"}}]
---

# Preserve checked cancellation-summary iterators

The native BPR-1901 baseline fails in Windows Debug because `std::ranges::copy`
returns MSVC's checked array iterator and the cancellation logger declares it
as a raw pointer. Preserve its actual iterator type; retain the bounds check,
copied text, length and sanitized classification.

- AC1: The affected application host compiles with native checked MSVC Debug
  and native Release without changing diagnostic options.
- AC2: Existing window-read host cancellation and diagnostic assertions pass.
- AC3: Land the correction on main with a one-line journal entry, and use the
  identical correction in both sides of BPR-1901's measurements.

## Verification

Source correction: `a0ca632a1`. The native baseline at `a73e25720` failed
MSVC C3535/C2440 with `_Array_iterator<char,2048>`; its raw failure is retained.
Applying the identical two-line correction to that isolated checkout passes
the selected native Debug and Release builds and
`application_github_window_read_host` (2.83 s and 0.31 s). Those timings are
functional-course observations, not application performance acceptance.
Clang-format 22, the documentation gate and the complete-link source controls
pass; the latter's generated-graph check is explicitly skipped on the Mac.
No compiler options or test assertions change. Source is integrated on main.

Evidence remains outside Git at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-windows-rebuilds-gl2kqegv`
and the task-owned native `D:/BPR-1901-gl2kqegv` checkout. The correction is
common to both sides of BPR-1901's pending comparison.
