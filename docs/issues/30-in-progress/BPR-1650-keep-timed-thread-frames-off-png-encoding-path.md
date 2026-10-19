---
schema: "implement-rfc.card.v1"
id: "BPR-1650"
title: "Keep timed thread frames off PNG encoding path"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/adapters_qt/workbench_window_tests.cpp"]}
---

# BPR-1650: Keep timed thread frames off PNG encoding path

At Windows2x, each synchronous PNG encode costs about400ms. The five timed captures starve animation events; original final elapsed2294ms retains arrival strength0.623457, and one processEvents clears it. The original final assertion is retained false in diagnosis.

## Acceptance

- AC1: Buffer raw timed frame grabs and serialize PNG after the timed course, pumping events at each capture boundary. Retain product animation1350ms, capture times500/950/1150/1700ms, exact filenames, raster success checks and final settled-pick assertion.
- AC2: Rebuild and run the complete thread-move courses at100/125/200 percent and shared workbench scale rows. Preserve all original six frames and native timing observations; do not change global waits, animation durations or test budgets.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.
