---
schema: "implement-rfc.card.v1"
id: "BPR-1649"
title: "State wide tray layout before label assertions"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/adapters_qt/workbench_window_tests.cpp"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"The label-bearing tray fixture states1600px window and host layout FilesWidth320/ConversationsWidth420. Product compact340px icon-only threshold, narrow layouts and all labels/count/focus/privacy/accessibility assertions remain unchanged.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-focused-recovered-r1/result.json"}},{"criterion":"AC2","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"Windows complete workbench100/125/200 PASS187.012/196.604/216.219s and current-file unread badge4.89385s, under original budgets. All corresponding Mac/Ubuntu offscreen rows pass; measured original327px compact state remains retained. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.","artifact":".mask/sessions/release-v0.3.0/linux-final-evidence/v030-linux-shared-fixtures-recovered-r1-results/junit.xml"}}]
---

# BPR-1649: State wide tray layout before label assertions

The actual327px tray correctly presents four icon-only tabs below the existing340px product threshold. The fixture asserts full text without specifying a wide host layout; icons, accessibility names, selection and visibility already match.

## Acceptance

- AC1: Supply an explicit wide host layout in the label-bearing tray fixture. Preserve the compact product threshold and every original label, count, focus, privacy, action and accessibility assertion; do not force a widget minimum or alter the product UI.
- AC2: Rebuild and run current-file unread badge plus all three complete workbench scale rows, retaining the original predicate/geometry evidence and existing narrow-layout coverage.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.

## Observed verification and closeout

Windows complete workbench100/125/200 PASS187.012/196.604/216.219s and current-file unread badge4.89385s, under original budgets. All corresponding Mac/Ubuntu offscreen rows pass; measured original327px compact state remains retained. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.

The changed C++ set passes clang-format22; the four shared fixture translation
units pass focused clang-tidy22. The Windows-only app observation compiles in
actual Debug and Release builds. The network verifier72 unit checks,
documentation67 tests and documentation/diff gates pass. Offscreen evidence
qualifies no native desktop or system appearance. Final producer source freeze
follows all card/ledger upkeep; remaining timing/native/vault/security and
source-access obligations stay with the coordinator.

The [canonical frontend-design skill](https://raw.githubusercontent.com/anthropics/claude-code/main/plugins/frontend-design/skills/frontend-design/SKILL.md)
guided the bounded Qt fixture repair; the established product layout and style remain.
