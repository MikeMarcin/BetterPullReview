---
schema: "implement-rfc.card.v1"
id: "BPR-2102"
title: "Count the agent picker's total twelve-pixel gap once"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"bpr-2102-picker-total-gap","path":"docs/rfcs/plans/bpr-2102-picker-total-gap/milestone-plan.json"},"release_source":"10cf9a61c1cdadc34084452b6735970e7b8c5b52","owner_scope":"release57-picker-total-gap"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"6843d548fe9427898cfe32425d6bf441e5ed8676","evidence":{"kind":"artifact","detail":"Scoped total-gap calculation accepted by root, original >=12 and exact threshold retained, main source408dc332b pushed."}},{"criterion":"AC2","outcome":"pass","revision":"6843d548fe9427898cfe32425d6bf441e5ed8676","evidence":{"kind":"artifact","detail":"Old57 normal archive with new boundary fixture rejects exact new assertion; corrected original Mac and Linux Release/Debug rows each pass once; all original interactions/oracles retained."}},{"criterion":"AC3","outcome":"pass","revision":"6843d548fe9427898cfe32425d6bf441e5ed8676","evidence":{"kind":"artifact","detail":"Root independent final acceptance binds1908Mac/1190Linux unchanged inputs, images/maps/strongsymbols/XML/markers/emptyforests; required local gates0 and scoped intentionalmain source pushed. Final common-sourceWindows/fullqualification remains separate."}}]
---

# BPR-2102: Count the agent picker's total twelve-pixel gap once

## Cause and mandate

Frozen57 Mac Release `qt_agent_picker_wrap` fails the original exact wrap threshold at light drawer540. One bounded scalar diagnostic reproduces it: available506, preferred widget widths sum488, ordinary spacing2 across four adjacencies, additional reserve12, production needed508. The independent original fixture needs506 including a total12 gap and expects an unwrapped row. Every widget's public effective width equals QWidgetItem preferred width; actual widget/layout boxes agree. This is first-party spacing accounting, not a Qt-library or timing waiver.

BPR-1924 states a12-pixel gap. Root approved subtracting the already-counted row spacing from the additional reserve, clamped at zero; the original >=12 mapped-gap and exact-threshold assertions remain. Frontend-design and its desktop reference were loaded. No visual redesign, new API, host effect or source57 mutation is authorized.

## Acceptance

- AC1: Count the total12 gap once while preserving preferred/minimum sizing, visibility and existing reflow/geometry policy.
- AC2: An exact boundary control distinguishes old57 double-counting and the adjacent width that cannot fit; all original ordinary/minimum/light/dark/hover/Tour oracles remain.
- AC3: Bind focused corrected native proofs, preserved original negative, scoped gates/root review and own main landing. Final corrected common-source Windows/full native qualification remains separate.

## Evidence and current state

Original57 Release22 selection has21 passes/one picker failure; other21 Debug rows pass and picker Debug was unrun. Private diagnostic compile0; collectorR1 helper lexical-path refusal before link/course retained; exact good object reused for one link0 and original course8/0.974409875s. All1779protected normal inputs/source57 remain exact and owned groups are empty. Raw evidence is external `implementer57-macos-picker-fit-triage-r1/`; causal-summary SHA25661a6d7f540b12a499417cbb6d7c35b80399b4651a2c020107aa8bb709f9ae14e. No diagnostic qualification credit.

## Implementation and acceptance

Tested private source `6843d548fe9427898cfe32425d6bf441e5ed8676` changes only the Qt fit calculation and its existing test TU. The additional reserve is `max(0, 12 - rowSpacing)` after the ordinary gaps have already been counted. Preferred/minimum sizing is unchanged. An exact-boundary control pins the five controls to their independent effective widths, requires the original mapped gap of at least twelve pixels, and requires wrapping one pixel below the exact fit.

Mac normal57 archive plus the new fixture rejects exactly the new boundary assertion (CTest8/0.809673s); corrected original Release/Debug rows pass once (1.94724s/1.63132s). Linux corrected original Release/Debug rows pass once (1.020694423s/1.016419019s). All original120s bounds, geometry oracles, interactions and native flags remain. Maps and strong symbols prove the corrected private Qt object replaces the old archive member. All1908Mac/1190Linux protected normal inputs remain exact; independent image-after hashes agree and owned forests are empty. Root independently accepted both native proof packets and the scoped source.

Format22, two-TU tidy22, architecture, documentation, 67documentation contracts, scoped board and explicit own-plan gates pass. The parent-plan discovery refusal (`unknown fields ['amendments']`) and first MASK identity refusal remain preserved; supported explicit own-plan validation and corrected own-checkout MASK identity repair pass. Neither refusal is relabelled a successful original command.

Canonical source commit `408dc332b094265d699ae069df29dac5e3a6de27` is pushed; only the tested minimal two-path patch was applied, preserving foreign main content. Final common-source Windows/full qualification belongs to the coordinator and remains separate. Frozen57 was not changed or published. The already-complete Ubuntu57 three graphs remain actual passes; its superseded Windows build was retired by exact owned identity during Debug (native125/outer1), with Release complete and distribution unreached.

See the [durable handoff](../_checkpoints/BPR-2102/handoff.md).
