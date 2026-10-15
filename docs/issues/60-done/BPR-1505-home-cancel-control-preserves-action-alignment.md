---
schema: "implement-rfc.card.v1"
id: "BPR-1505"
title: "Place Home Cancel left of Changes to preserve action alignment"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Put cancel on the left of changes etc on home so it doesn't move the alignment.","milestone_plan":{"id":"bpr1505-home-cancel","path":"docs/rfcs/plans/bpr1505-home-cancel/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"90958011c","evidence":{"kind":"observation","detail":"The composed Home public path measures actual Cancel and Retry widget bounds left of the matching row's change facts; inspected light and dark offscreen idle/waiting/failed renders show the control left of the stable facts."}},{"criterion":"AC2","outcome":"pass","revision":"90958011c","evidence":{"kind":"command","detail":"qt_home_list_scale_100/125/200 pass: facts left/right and Reviewing-tag left remain unchanged through opening, Cancel, Retry and idle. The final composed public course passes matching horizontal geometry assertions through opening, waiting, cancellation and failure/Retry."}},{"criterion":"AC3","outcome":"pass","revision":"90958011c","evidence":{"kind":"command","detail":"application_home_open_feedback_public_path passes; final network-isolated replay with --render-directory exits 0 and prints home-open-feedback-public-path:pass. Home double-click, delayed transport, Cancel wire stop and Retry are exercised. Light and dark renders inspected; clang-format 22, diff whitespace check and 67 documentation-verifier tests pass."}}]
---

# BPR-1505: Place Home Cancel left of Changes to preserve action alignment

Move Home's Cancel control to the left of Changes and the adjacent actions so showing or hiding Cancel does not move their alignment. Preserve the existing cancellation behavior.

## Acceptance

- AC1: Cancel appears to the left of Changes and the adjacent Home actions.
- AC2: Showing and hiding Cancel leaves the other actions aligned in the same positions.
- AC3: The existing cancellation action continues to work; verify the rendered Home state offscreen and exercise the relevant interaction path.

## Implementation

Load frontend-design:frontend-design before UX work, as required by AGENTS.md. Keep scope to this Home layout correction. Do not capture or operate the owner's live desktop.

## Execution record

Candidate `90958011c` keeps the idle right-hand action width when a row opens.
Cancel and Retry use a separate space immediately left of the Reviewing tag,
account chip and change facts, so their labels do not move the trailing columns.
The frontend-design skill was loaded; the arrangement preserves the existing
native font, palette, control style and spacing.

- AC1: The composed Home course measures the actual Cancel and Retry widgets
  left of the matching row's facts. Inspected light and dark offscreen
  idle, waiting and failed renders show the arrangement.
- AC2: Delegate geometry checks keep both facts edges and the Reviewing tag's
  left edge unchanged through opening, Cancel, Retry and idle. The composed
  course compares the row's horizontal facts geometry before opening, during
  opening and waiting, after cancellation, and on failure with Retry.
- AC3: `application_home_open_feedback_public_path` passes. The final candidate
  also passes that network-isolated course with `--render-directory`, through
  the actual Home double-click, delayed transport, Cancel wire stop and Retry
  gestures. `qt_home_list_scale_100`, `_125` and `_200` pass serially.

The three changed C++ files pass the canonical clang-format 22 check and
`git diff --check`. Focused clang-tidy passes with the existing
`reviewing_course` naming warning in the public test. The documentation
verifier's 67 self-tests pass.
The full documentation gate is pending coordinator integration: the authorized
metadata import introduces BPR-1521 as Ready, but this branch's older baseline
still has its prerequisite BPR-1520 in Ready. BPR-1505's scoped board validation
is separate from that unrelated dependency state.

Documentation impact: this is a local Qt geometry correction. Existing public
commands, cancellation semantics, core policy, effects and stored state have
no semantic change; this execution record documents the revised placement.
