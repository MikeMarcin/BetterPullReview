---
schema: "implement-rfc.card.v1"
id: "BPR-1516"
title: "Stop vertical diff text shake while dragging the pane divider"
status: "Done"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"There's a weird vertical shake of the diff text panes when dragging the divider to resize them","dispatch":"Dedicated implementer completed diagnosis, fix, focused verification and coordinator public-path integration","milestone_plan":{"id":"bpr-1516-divider-viewport-stability","path":"docs/rfcs/plans/bpr-1516-divider-viewport-stability/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"accf697f92fe20aff2f593e43db217d3f84f2d9b","evidence":{"kind":"observation","detail":"Original actual reversed-divider drag recorded8px/38vertical rowchanges; independent offset reset negative control5px/78changes on2002/2003lines.","checkpoint":"docs/issues/_checkpoints/BPR-1516/checkpoint.json","integrated_revision":"a8b37e46c"}},{"criterion":"AC2","outcome":"pass","revision":"accf697f92fe20aff2f593e43db217d3f84f2d9b","evidence":{"kind":"observation","detail":"Final actual continuous reversed drags at1.0/1.25/2.0 show0px/0changes and39rebuilds; native longest-line relayout feedback removed, fractional placement retained between batches.","checkpoint":"docs/issues/_checkpoints/BPR-1516/checkpoint.json","integrated_revision":"a8b37e46c"}},{"criterion":"AC3","outcome":"pass","revision":"accf697f92fe20aff2f593e43db217d3f84f2d9b","evidence":{"kind":"observation","detail":"Native selection/cursor,source hit identity,shared q,horizontal ranges and exact ribbon correspondence pass for unequal short/long/large sources. Full native measurement suite covers wrapped/Unicode/mixed-font boundaries.","checkpoint":"docs/issues/_checkpoints/BPR-1516/checkpoint.json","integrated_revision":"a8b37e46c"}},{"criterion":"AC4","outcome":"pass","revision":"accf697f92fe20aff2f593e43db217d3f84f2d9b","evidence":{"kind":"observation","detail":"Actual diff_link_map press/move/reverse/release through composed diff_workbench, per-paint/yield row measurements and offscreen renders pass. Integrated exact maina8b37e46c full-window public app path plus Qt1.25 pass; coordinator inspected raster.","checkpoint":"docs/issues/_checkpoints/BPR-1516/checkpoint.json","integrated_revision":"a8b37e46c"}}]
---

# BPR-1516: Stop vertical diff text shake while dragging the pane divider

Dragging the divider to resize the diff text panes causes visible vertical shake. Reproduce the sequence and correct the underlying layout/scroll feedback so horizontal resizing keeps a stable vertical view.

## Acceptance

- AC1: Reproduce and record the vertical displacement during representative continuous divider drags, including reversed drag direction.
- AC2: Resizing the panes no longer introduces repeated vertical oscillation or jumps; legitimate reflow preserves the intended viewport anchor under the existing comparison-space scroll authority.
- AC3: Correspondence, synchronized scrolling, source line identity and selection remain correct after resize, including unequal sides, wrapped lines and large diffs as applicable.
- AC4: Exercise actual divider drag events through the composed diff widget with objective viewport/anchor observations and offscreen rendered evidence. Avoid a fix that only conceals a scroll or layout feedback loop.

## Coordination

BPR-1512 changes the center ribbon width and may touch the splitter; coordinate ownership and integration. Load frontend-design:frontend-design before UX edits per AGENTS.md. Never drive or capture the owner’s live desktop. Required human judgement goes through a short attended session and exact frozen candidate.


## Diagnosis and verified candidate

Source candidate `accf697f92fe20aff2f593e43db217d3f84f2d9b` removes two causes.
Native width relayout in an unwrapped pane temporarily forgot its longest line,
which hid and restored its horizontal scrollbar. Measured viewport heights
alternated between 601 and 583 logical pixels; the shared 0.43 synchronization
rail therefore moved about 8 pixels while comparison position stayed fixed.
The pane now retains its width-independent native layouts and still lets Qt
adjust its scrollbars from the actual viewport. Bounded remeasurement also
retains fractional placement until the shared controller applies the new axis.

- Reproduction: Actual center-handle mouse press, forty continuous forward/reverse moves
  and release reproduced 38 vertical row changes, up to 8 logical pixels, on
  the original 202/203-line comparison at 1.25 scale. A targeted negative
  control restoring only the old measurement offset reset produced 78 changes
  and 5 pixels of displacement on 2,002/2,003 lines.
- Stability: The final candidate has zero displacement and zero row changes in every
  observed paint and yielded measurement turn. Each effective move has one
  geometry rebuild: 39 per course, compared with 58 on the original long-line
  baseline. No paint suppression or competing scroll authority was added.
- Correspondence: Short and long files, unequal sides, the retained semantic line and
  native text-selection range, native hit testing, shared comparison position,
  horizontal ranges and exact ribbon endpoints pass. The shipped readers are
  unwrapped; the wrapped/Unicode/mixed-font native measurement regression also
  passes through `qt_diff_axis_scale_100`.
- Composition: The real `diff_workbench` and its `diff_link_map` receive actual pointer
  events. Offscreen before/wide/reversed renders and per-paint row observations
  cover 201/202, 202/203 and 2,002/2,003-line fixtures at 1.0, 1.25 and 2.0 scale.
  Coordinator main `a8b37e46c` includes this source with BPR-1512 ribbon width: full-window `application_diff_workbench_public_path` passes (4.28 s), and `qt_diff_workbench_scale_125` passes (15.94 s). The coordinator inspected the full-window render: selected paired lines 39/42, ribbons, gutters, Comments, text boundaries and horizontal bars remain correct.

Focused verification: `qt_diff_workbench_scale_100`,
`qt_comparison_scroll_controller_scale_100` and `qt_diff_axis_scale_100` pass
(3/3). The isolated divider course passes at 1.25 and 2.0. Clang-format 22.1.8
on both touched C++ files, `git diff --check`, `verify_documentation.py` and all
67 documentation-verifier tests pass. The initial documentation gate correctly
refused the stale issue counter; the coordinator-authorized metadata import
restored the authoritative counter before the passing run.

Frontend-design guidance was loaded. Native typography, palette and comparison
layout are preserved. No host/core API, provider, persistence, authorization or
semantic correspondence rule changed. Evidence paths, digests, command state
and the original mandate are in
`docs/issues/_checkpoints/BPR-1516/checkpoint.json`.
