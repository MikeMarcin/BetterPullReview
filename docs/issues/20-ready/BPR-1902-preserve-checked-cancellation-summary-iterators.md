---
schema: "implement-rfc.card.v1"
id: "BPR-1902"
title: "Preserve checked cancellation-summary iterators"
status: "Ready"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"discovered_by":"BPR-1901","milestone_plan":{"id":"windows-checked-iterator-v1","path":"docs/rfcs/plans/windows-checked-iterator-v1/milestone-plan.json"}}
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
