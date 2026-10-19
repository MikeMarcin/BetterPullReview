---
schema: "implement-rfc.card.v1"
id: "BPR-1651"
title: "Click inside the full footnote reference span"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["tests/adapters_qt/markdown_github_tests.cpp"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"The pointer targets the full selected first footnote-reference span midpoint with an interior-anchor prerequisite. Real press/release, keyboard traversal, exact back references, zero in-body gestures and authored-link negative control remain; no Markdown product change.","artifact":".mask/sessions/release-v0.3.0/windows-diagnosis/v030-windows-focused-recovered-r1/result.json"}},{"criterion":"AC2","outcome":"pass","revision":"f164d6214664111c27eb4e89a33d68fce293925e","evidence":{"kind":"observation","detail":"All three full Markdown GitHub scale rows pass on Windows, Mac and Ubuntu, including original keyboard/pointer/negative controls. Original boundary x139/ExactHit24 versus selectionStart25 and interior midpoint143 observation remains retained. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.","artifact":".mask/sessions/release-v0.3.0/linux-final-evidence/v030-linux-shared-fixtures-recovered-r1-results/junit.xml"}}]
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

## Observed verification and closeout

All three full Markdown GitHub scale rows pass on Windows, Mac and Ubuntu, including original keyboard/pointer/negative controls. Original boundary x139/ExactHit24 versus selectionStart25 and interior midpoint143 observation remains retained. Original780-row Windows gate at93897 remains767 pass/12 fail/1 configured Debug Skip, with full raw evidence retained. Focused native16/16 at1cdaf repairs all12 failures; finalf164 metrics and exact resealed composition follow-ups pass. All changed shared fixtures pass Mac13 rows plus final metrics/tidy and two recovery consumers, and Ubuntu15 rows atf164. Relevant input bytes are unchanged through this closeout; no full normal rerun or native desktop qualification is inferred.

The changed C++ set passes clang-format22; the four shared fixture translation
units pass focused clang-tidy22. The Windows-only app observation compiles in
actual Debug and Release builds. The network verifier72 unit checks,
documentation67 tests and documentation/diff gates pass. Offscreen evidence
qualifies no native desktop or system appearance. Final producer source freeze
follows all card/ledger upkeep; remaining timing/native/vault/security and
source-access obligations stay with the coordinator.

The [canonical frontend-design skill](https://raw.githubusercontent.com/anthropics/claude-code/main/plugins/frontend-design/skills/frontend-design/SKILL.md)
guided the bounded Qt fixture repair; the established product layout and style remain.
