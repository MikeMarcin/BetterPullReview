---
schema: "implement-rfc.card.v1"
id: "BPR-1512"
title: "Make center ribbon width configurable and halve its default"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Make the center ribbon width a setting and make the default half the current width","milestone_plan":{"id":"bpr1512-ribbon-width","path":"docs/rfcs/plans/bpr1512-ribbon-width/milestone-plan.json"}}
---

# BPR-1512: Make center ribbon width configurable and halve its default

Expose the center ribbon width through the existing settings workflow. Set the default to half the current default width. Inspect the actual center ribbon implementation and measure its existing width before choosing the new value.

## Acceptance

- AC1: The center ribbon width is an accessible user setting with sensible supported bounds and a clear unit/label consistent with existing settings.
- AC2: A new/default configuration uses half the previous default center ribbon width. Reset to default selects this new value; preserve existing explicit user choices if the setting already exists in some form.
- AC3: Changing the setting updates the actual center ribbon and persists through the established settings lifecycle and restart.
- AC4: The narrower default and supported widths preserve usable ribbon interaction, adjacent panes and rendering. Verify the real offscreen workbench/settings path and focused persistence/default checks.

## Implementation

Load frontend-design:frontend-design before UX work as required by AGENTS.md. Follow core policy, host effects and Qt presentation boundaries. Coordinate any shared workbench/settings paths with other active testing-note cards. Never drive or capture the owner’s live desktop. Required visual judgement uses a short attended session and exact frozen candidate.


## Milestone checkpoint — owner-2026-10-14-r1

Assignment: BPR-1512 only, baseline `3c344e31b52fedf2ee45b863d53f8916373299e5`,
branch `bpr-1512-ribbon-width`, isolated worktree
`/Users/jaeger/.codex/worktrees/bpr1512-ribbon-width/BetterPullReview`.
The mandate is the owner's note: “Make the center ribbon width a setting and
make the default half the current width”. No prior milestone handoff exists.
The adopted plan is `bpr1512-ribbon-width`, milestone `configurable-ribbon`.

Read the implement-rfc engineering contract and coordinator-implementer recipe,
consumer context, governing settings/diff RFCs and frontend-design skill before
editing. Scoped MASK registration completed through its exact reviewed plan;
preflight reports `ready_pinned`, configured Codex route `gpt-6.1-sol`/`xhigh`.
This reports deployment readiness, not observed host loading. Canonical claim:
`0ecc85117d0f352cf8ce350f32f64c345e571526`.

Design: retain the native font, palette, row layout and reset pattern. Add one
spinbox under Reviews & queue → Comparison, labeled Center ribbon width with
`px` and an accessible logical-pixel explanation. No new color, typography or
surface treatment is required for this preference. The existing production
splitter and map configuration both defaulted to **56 logical pixels**. The
new default is **28**, with supported bounds **16–256**. Five-pixel move markers
fit the lower bound. The setting did not previously exist in persisted state.

Implementation owns the profile preference, accepted publication to the real
splitter and schema-20 storage migration. It preserves older profile/window
choices and new explicit widths across future writes. BPR-1516 owns divider
shake: this slice changes no drag threshold, scroll authority or remeasurement
policy. Other agents' Home, Seen, Accept, Search, tab, Cmd+J and unread-badge
work remains outside this branch's source scope.

Unattended result: focused dev build completed. The real settings/workbench
width course passes at 100%, 125% and 200%; the rendered 28-pixel ribbon and
native Settings row were inspected. Profile-preference lifecycle, SQLite
storage/migration/default/bounds/reopen and all three link-map scales pass.
The shipped selected-file composition contract passes. Both architecture gates
and all 67 documentation-verifier tests pass. The initial build's missing
preference-header include and the first storage run's missing admission-range
case were corrected and rerun. The UI course explicitly enters Review with
Files and Comments open and Repositories collapsed, before measuring source
panes. Required AC4 human judgement remains pending.

Next: freeze this verified source into its own source/build root and run the
launcher's own check, then create a fresh short attended session. The
coordinator owns subsequent review intake and integration. The documentation
gate's initial run is refused solely by shared counter high-water drift (local
NEXT_ID 1515, shared historical high water 1520); the coordinator owns further
imports. Do not edit or rewind NEXT_ID to bypass this refusal.


## Verified candidate and attended handoff

Product candidate: `7586722a31c88ccae46467c15dc61f98cc20167c`.
Changed paths are the explicit nineteen paths of that commit, enumerated with
commands and exact candidate/session identities in the task-owned runtime
checkpoint `.mask/sessions/bpr1512-ribbon-v1/handoff.json`. Local logs and
renders remain under its `evidence/` directory and are not committed.

| Acceptance | Evidence at the product candidate | Result |
|---|---|---|
| AC1 | `qt_center_ribbon_width_scale_1.0`, `1.25`, `2.0`: actual Settings spinbox, accessible label/description, unit and bounds | Pass |
| AC2 | Actual map/default/reset 28; previous splitter/config 56; exact schema-19 migration keeps prior choices | Pass |
| AC3 | `application_workbench_preferences_consumers`; `sqlite_workbench_settings_storage`: durable publication/fan-out/failure/coalescing and explicit widths through close/reopen | Pass |
| AC4 automated | Three workbench scales; `qt_diff_link_map_scale_100`, `125`, `200` including drag and hit identity at 16/28/56/256; `application_workbench_composition_contract` | Pass |
| AC4 human | Exact frozen candidate below; keep/change judgement | Pending |

Additional verification: final `architecture_boundaries` passes (14.93 s),
`architecture_boundary_contract` passes (208.76 s), touched C++ format check
uses clang-format 22.1.8, `git diff --check` passes, and the documentation
verifier's 67 regression tests pass. The source changes do not require a new
core policy or dependency.

Frozen source and build are self-contained in
`/Users/jaeger/bpr1512-ribbon-v1.91gv2rpw`. Launch:
`python3 /Users/jaeger/bpr1512-ribbon-v1.91gv2rpw/launch.py`.
The freeze tool ran that launcher's own offscreen `--check` successfully:
`ATTENDED_FILE_SHOWN:review.cpp`, fixture head
`f9108913c053354eab2e4771eef27e957a400966`. Executable SHA-256:
`7db700961810f03725b1ae24f39976ac855ba41bb74b53a721b0234d57099903`.

Fresh short attended session: `d3d7173485ff738019fd10530fb5290a`, cursor **4**,
state `/Users/jaeger/.codex/bpr1512-review-v1.rse5c50d`.
Candidate `BPR-1512-ribbon-v1`, fingerprint
`13c1bbadf06e4135222a1fccdaba12fde18ccf7ffe2ad1a2195f6b8edee1c8ba`, is ready,
protected and unanswered. The complete loopback URL is retained only in the
runtime handoff, never in committed documentation. It contains an offscreen
workbench render, the native Settings control render, the exact frozen launch
command and one Accept/Reject question for AC4.

Canonical `finish` with the `human-pending` decision was attempted using the saved
partial acceptance file and refused solely by the shared-counter guard:
`BPR-1515 rewinds historical high water BPR-1520`. The card therefore truthfully
remains In Progress until the coordinator imports the subsequent reserved
cards/counter/receipts and retries canonical finish. The documentation gate has
the same outstanding counter refusal; no counter was edited or override added.

All task-owned configure/build/test/freeze processes have completed. No other
writer owns this isolated worktree. The sole retained command is the expressly
handed-off review server, PID **45844**, command session **63255**, port **54464**.
No owner desktop was driven or captured; all renders and the launcher check were
offscreen. Frontend-design was loaded and applied to the narrow native-control
addition. Keep the worktree, frozen root and server until review reconciliation.

Next action: coordinator integrates claim and product/checkpoint commits,
imports the current tracker state under its own authority, runs the
documentation gate and canonical human-pending finish, presents the complete
review URL and owns `wait --after 4 --kind handed-back`. Review interpretation
belongs to the next engineering round; no verdict is inferred here.
