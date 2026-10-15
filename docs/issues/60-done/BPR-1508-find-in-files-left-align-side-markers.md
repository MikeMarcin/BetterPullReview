---
schema: "implement-rfc.card.v1"
id: "BPR-1508"
title: "Start Find in Files L/R markers at the far left"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Find in files we can start the LR markers on the far left they don't need to be indented","milestone_plan":{"id":"bpr1508-search-markers","path":"docs/rfcs/plans/bpr1508-search-markers/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"df00148b686bc2561b59ca257dadb14d035e2b28","evidence":{"kind":"observation","detail":"Offscreen 1x/2x panel raster and fixed-column checks: match rectangle x=0, tag slots x=4 with normal content padding; left and right fills verified.","ref":"docs/issues/_checkpoints/BPR-1508/handoff.md"}},{"criterion":"AC2","outcome":"pass","revision":"df00148b686bc2561b59ca257dadb14d035e2b28","evidence":{"kind":"observation","detail":"Marker clicks select/activate exact rows; Up/Enter and native group collapse/expand pass. Composed Enter/F4 reveal exact left/right spans and focus, with filtering, counts and no-rescan traversal preserved.","ref":"docs/issues/_checkpoints/BPR-1508/handoff.md"}},{"criterion":"AC3","outcome":"pass","revision":"df00148b686bc2561b59ca257dadb14d035e2b28","evidence":{"kind":"observation","detail":"Real shipped installed-Git/cache fixture composition rendered left/right results and passed application_source_pr_find at 1x/1.25x/2x. Full-window/panel rasters inspected. Baseline negative control fails new left-edge assertion, restored candidate passes.","ref":"docs/issues/_checkpoints/BPR-1508/handoff.md"}}]
---

# BPR-1508: Start Find in Files L/R markers at the far left

Remove the unnecessary leading indentation before the L/R side markers in Find in Files results, placing those markers at the far left of their result rows.

## Acceptance

- AC1: Find in Files result L/R markers begin at the far left without the current unnecessary indentation.
- AC2: Result text remains legible and aligned, with existing grouping, side identity, selection, keyboard navigation and reveal behavior preserved.
- AC3: Verify representative left and right results in an offscreen rendered application state and exercise the relevant result interaction.

## Implementation

Load frontend-design:frontend-design before UX edits as required by AGENTS.md. Keep scope to Find in Files result alignment. Never operate or capture the owner’s live desktop. Prepare a short attended review with an exact frozen candidate if human judgement remains required.


## Result and evidence

Mandate `owner-note-2026-10-14-bpr1508`; product candidate
`df00148b686bc2561b59ca257dadb14d035e2b28`.
The [durable milestone checkpoint](../_checkpoints/BPR-1508/handoff.md) records
the exact scope, candidate blobs, plan/binding, commands, evidence, temporary
negative control and integration instructions.

- AC1 passes: match row geometry starts at x=0, with the existing four-pixel
  content inset before fixed L/R slots. Colored marker pixels confirm the
  delegate paints there at 1x/2x.
- AC2 passes: native file arrows still collapse/expand groups; marker clicks,
  selection, Up/Enter, fixed number/snippet columns, side filters and counts pass.
  The composed application retains exact left/right source reveal and F4 focus.
- AC3 passes: real installed-Git/cache fixture composition renders both result
  sides and passes at 1x, 1.25x and 2x. The original layout fails the new left-edge
  regression; restoring/rebuilding the exact correction passes. A post-commit
  rendered panel plus composed application pass is bound to the product commit.

The required frontend-design skill was loaded. The architecture source-search
contract documents the alignment; semantic policy, effects and persistence have
no impact. No human-only acceptance criterion remains, so no attended session or
frozen build is required. Offscreen macOS evidence does not claim other native
platform, system-appearance or accessibility qualification.

All task-owned build/test commands have completed; there is no writer or review
server to hand off. The coordinator integrates the scoped BPR-1508 commits and
reconciles the adjacent e2e hunk with BPR-1509. No further BPR-1508 work is pending.
