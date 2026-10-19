---
schema: "implement-rfc.card.v1"
id: "BPR-1659"
title: "Share the Overview button treatment and migrate Settings"
status: "Done"
rfcs: ["RFC-BPR-0087"]
depends_on: []
metadata: {"source": "Owner request 2026-10-19; isolated worktree, no merge/push/release", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
acceptance: [{"criterion":"BPR-1659:reference","outcome":"pass","revision":"9dc946997350ab23866ce75f39bbebf32b140be8","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Overview Open/All retains its accepted content/geometry and uses button_surface. Registered Overview course and four-theme switch checks pass under both styles at three scales; full-page/switch renders inspected."}},{"criterion":"BPR-1659:settings","outcome":"pass","revision":"9dc946997350ab23866ce75f39bbebf32b140be8","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Review/Comments choice and Reset factories and Appearance tab-width choices use the shared treatment. Existing names, commands and specialized controls preserved. Four-theme composed Settings state checks and Appearance tab-choice interaction/captures pass."}},{"criterion":"BPR-1659:verification","outcome":"pass","revision":"9dc946997350ab23866ce75f39bbebf32b140be8","evidence":{"kind":"observation","path":"docs/verification/RFC-BPR-0087.md","detail":"Offline isolated build linked six targets. Focused CTest 10/10; style/scale matrix 18/18. Hover/press/focus/disabled renders inspected. Tidy/format/docs/whitespace pass. Unrelated full Appearance 2x offscreen popover failure reproduced with native-button control and retained; native desktop integration not claimed."}}]
---

# BPR-1659: Share the Overview button treatment and migrate Settings

## Acceptance

- Reference Open/All uses the shared surface painter without losing its accepted appearance or behavior.
- Review and Comments choices/Reset plus Appearance tab choices use the same treatment; primary and specialized controls retain their meanings.
- Four-theme focused rendering, interaction, disabled/focus/hover checks and relevant composed tests pass; record exact commands and platform limits.

## Execution

The current delegated implementer owns the first milestone on
`codex/shared-action-buttons`. The diagnostic audit is independent and read-only.
Do not change release work or merge, push or release.

## Source checkpoint — 2026-10-19

Initial source commit `2660e4c63096479bb5d78090017e9f0e8d898e28` extracts
`button_surface`, adds `quiet_tool_button`, and migrates the Review/Comments
choice/Reset factories and Appearance tab-width choices. The reference Open/All
keeps its own content. The initial app-wide inventory and composed regression
test source are written.

The user's disk-space steering stopped large build/download work before it
started; about 1.0–1.1 GiB is free. This task has no build processes or large
build outputs. Source is preserved in
`/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons`, branch
`codex/shared-action-buttons`.

All six touched translation units pass syntax-only checks. The new implementation
passes clang-tidy 22; all touched C++ passes clang-format 22; architecture,
documentation and whitespace gates pass. No build/link/runtime pass or new
visual verdict is claimed. [Evidence and exact continuation](../../verification/RFC-BPR-0087.md).

Resume with the focused build and serial runtime/visual checks once disk
headroom is confirmed. Do not finish this card from source/syntax evidence alone.

## Verified resume — 2026-10-19

Candidate `9dc946997350ab23866ce75f39bbebf32b140be8`: offline isolated -g0
build succeeds; 10/10 focused Qt/composition tests and 18/18 serial
Fusion/Windows-style checks at 1x/1.25x/2x pass. Four-theme Review/Comments,
Overview and Appearance renders inspected, including hover, press, focus and
disabled states. The unrelated full Appearance suite's 2x offscreen popover
placement failure reproduces with the original native tab-width buttons and
is retained in the verification ledger; the normal-scale full suite passes.

Disk-space blocker cleared (about 54 GiB free after owner cleanup). No native
Windows/Linux or macOS system appearance claim; all new captures are offscreen.
The owner explicitly authorized continuing the planned RFC rollout.
BPR-1660 retains the remaining app-wide migration; no merge/push/release.
