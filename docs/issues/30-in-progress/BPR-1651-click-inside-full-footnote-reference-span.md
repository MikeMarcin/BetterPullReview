---
schema: "implement-rfc.card.v1"
id: "BPR-1651"
title: "Click inside the full footnote reference span"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/adapters_qt/markdown_github_tests.cpp"]}
---

# BPR-1651: Click inside the full footnote reference span

The Windows pointer x139 lies exactly before the selected footnote reference: anchorAt is empty and ExactHit24 precedes selectionStart25. The full selected span midpoint143 hits #fn-1; keyboard navigation and all application gesture controls already pass.

## Acceptance

- AC1: Derive the pointer from the full first-reference selection span instead of a caret-minus-two boundary. Keep real press/release dispatch, keyboard traversal, exact back-reference selection, zero in-body application gestures and authored-link negative control.
- AC2: Rebuild and run all three complete Markdown GitHub scale rows with actual interior anchor prerequisite and retain original hit/selection observations. No Markdown product behavior, font, scale or selection assertion changes.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.
