---
schema: "implement-rfc.card.v1"
id: "BPR-1524"
title: "Restore Windows source navigation latency during cold PR search"
status: "Ready"
rfcs: ["RFC-BPR-0077"]
depends_on: ["BPR-1523"]
priority: "P1"
metadata: {"source":"0.2.1 native release preflight","assigned_implementer":"/root/release021_review_controls","baseline":"6230bd1e37dfdb9c00d35d9a43ccafe2878bf535","failure_candidate":"dcc5ace22d16583f32b48fe71a5f1842c214d2d0","milestone_plan":{"id":"bpr-1524-windows-search-navigation","path":"docs/rfcs/plans/bpr-1524-windows-search-navigation/milestone-plan.json"}}
---

# BPR-1524: Restore Windows source navigation latency during cold PR search

The public large-review search course reproducibly records selected-file navigation at 3188 and 3142 ms on the Windows producer, exceeding its unchanged 2000 ms budget. All first/final cold/warm, cancellation and memory budgets pass. BPR-1523 exposed and retained this independent performance failure while completing native compiler and badge geometry portability. Its historical acceptance remains intact.

## Acceptance

- AC1: Preserve both exact-source Windows failures, use the shortest public composition to identify the delayed stage and production cause, and distinguish measured facts from hypotheses.
- AC2: Correct that production cause within explicitly assigned paths. Preserve every committed threshold, original public-path assertion, exact source/authority/cancellation boundary and truthful full-corpus search coverage.
- AC3: Verify the corrected committed candidate on Windows with navigation within 2000 ms and all other budgets passing; run relevant focused Mac public composition, format22, documentation and changed-boundary checks. Return clean main and producer source with commands quiescent. Full normal release gates, packaging/signing and publication remain coordinator obligations.

## Ownership and execution

The owner requires implementers to perform release fixes. Engineering owner `/root/release021_review_controls` initially owns timing diagnostics in `tests/end_to_end/source_pr_search_performance_tests.cpp`; the coordinator must receive the precise proposed production paths before their assignment and edit. Thresholds are not editable. Existing frontend-design is loaded if the diagnosed path affects Qt mechanics; no new visual design or acceptance is inferred.

Main and native/Mac producers were released clean by the BPR-1523 implementer at `6230bd1e37dfdb9c00d35d9a43ccafe2878bf535`. PC Windows and WSL build/test work is serialized. The separately owned Azure console/login wizard remains outside this issue. No owner desktop, profiles, installed apps, browser data, secrets, authentication settings, signing or remote publication are touched.
