---
schema: "implement-rfc.card.v1"
id: "BPR-2102"
title: "Count the agent picker's total twelve-pixel gap once"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"bpr-2102-picker-total-gap","path":"docs/rfcs/plans/bpr-2102-picker-total-gap/milestone-plan.json"},"release_source":"10cf9a61c1cdadc34084452b6735970e7b8c5b52","owner_scope":"release57-picker-total-gap"}
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

Implementation and native positives pending.
